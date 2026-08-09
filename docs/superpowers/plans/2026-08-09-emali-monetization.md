# eMali Manual Payment (MahlanyaRPG `web/`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a fan pay a fixed amount via Swazi Mobile E-Mali (manual reconciliation), submit a claim through a new `web/app/access/page.tsx`, and let the site owner manually confirm it from an admin page before a 7-day signed download link for the supporter build is emailed to them.

**Architecture:** A brand-new, isolated Supabase project holds a `payment_references` table and a private `mahlanya-builds` Storage bucket. `web/` gets its first-ever backend: a public submit API route (rate-limited, zod-validated, inserts a `pending` row, emails the owner via Resend), and shared-secret-gated admin API routes (list + confirm/reject) that mint a fresh 7-day Storage signed URL and email it to the payer on confirm. No browser-side Supabase key is ever issued — every DB/Storage operation happens server-side with the service-role key.

**Tech Stack:** Next.js 14.2 App Router, TypeScript 5.5, `@supabase/supabase-js`, `resend`, `zod`. No test runner exists in this app (`web/package.json` has no jest/vitest) — verification is `npm run typecheck`, `npm run build`, and manual dev-server checks with `curl`, matching this repo's existing tooling rather than introducing a new test framework for one feature.

## Global Constraints

**Parent `~/my-projects/MahlanyaRPG/CLAUDE.md` hard lines — applicability determined against actual CI wiring, not assumed:**

1. Historical accuracy CI gate (`historical_accuracy_check.py` / `pipeline/history/validate_content.py`) — **does not apply.** `.github/workflows/content-validate.yml` triggers only on `Content/Dialogue/**` and `pipeline/history/data/**`. This plan touches neither.
2. Performance regression bounds (±15%, 19 CVars, 5 tiers) — **does not apply.** These are UE5 rendering CVars measured by `.github/workflows/phase10-validation.yml`, which triggers only on `Source/MahlanyaRPG/Performance/**` and `Source/MahlanyaRPG/Core/MahlanyaLogChannels.*`. `web/` has no UE5 process.
3. No Nanite bypass — **does not apply.** Nanite is UE5 static-mesh rendering. `web/`'s Three.js scene (`components/HeightmapViewer.tsx`) uses `PlaneGeometry`, not `.uasset` meshes, and this plan adds no 3D geometry.
4. Zig SIMD owns erosion/hydrology math — **does not apply.** Nothing in this plan computes or ports erosion/hydrology logic; the viewer's existing fog overlay is untouched.
5. Cultural review before merge on settlement geometry — **does not apply.** This plan adds payment/admin UI copy only — no settlement geometry, no lore content. Confirmed explicitly rather than assumed: no village, NPC, or geometry files are touched.
6. UE5 commandlet determinism — **does not apply.** `Source/Mahlanya/Commandlets/*` is untouched.
7. 316 tests must pass (`pipeline/tests/` via `.github/workflows/pipeline-ci.yml`) — **does not apply.** That workflow triggers only on `pipeline/**` paths. This plan adds zero files under `pipeline/`, so the suite's file count and pass rate are unaffected.
8. Steam achievements via SDK — **does not apply.** No achievement system touched.

**Conclusion:** none of the 8 hard lines gate this work. Full reasoning and CI-file evidence in `docs/superpowers/specs/2026-08-09-emali-monetization-design.md`.

**Model routing (per `CLAUDE.md`'s "Sonnet for UI... asset pipeline work" bucket, with two judgment-call escalations):**
- Task 1 (Supabase schema + private storage bucket, from scratch) — **Opus.** No existing pattern in this repo to extend; a security-boundary design decision.
- Task 5 (admin auth + signed-URL minting + fail-closed delivery) — **Opus.** Same reasoning — the confirm action is the one place money and a real download link meet.
- All other tasks — **Sonnet** (mechanical UI/API implementation against a complete spec).

**Style constraints, drawn from the existing `web/` code, not invented:**
- Inline `style={}` objects, not Tailwind or CSS modules — `web/` has no CSS framework installed; `app/page.tsx` and `components/HeightmapViewer.tsx` both use inline styles. Match that.
- `'use client'` directive on any component using hooks/browser APIs, matching `components/HeightmapViewer.tsx`.
- No comments unless the WHY is non-obvious (repo-wide convention already followed in `HeightmapViewer.tsx`, e.g. its "Minimal orbit-style drag control" comment).

**Money and identifiers:**
- Amount stored as integer cents; currency default `SZL`.
- eMali payee number: `+26879657744`. Owner notification recipient: `charleskris9@gmail.com`.
- `SUPPORTER_TIER_PRICE_CENTS` is a single named constant (placeholder `5000` = E50.00) — the real price is an operator decision outside this plan; flagged, not silently assumed.

**Security:**
- No `SUPABASE_SERVICE_ROLE_KEY`, `RESEND_API_KEY`, or `ADMIN_EMALI_SECRET` ever referenced in a client component or `NEXT_PUBLIC_`-prefixed variable.
- No auto-confirm on submission. Status starts `pending`; only the authenticated confirm route can move it to `confirmed`/`rejected`.
- Admin routes fail closed: missing/wrong `Authorization: Bearer` header → 401, never a default-allow path.
- If the delivery email fails after confirm, the row stays `pending` (not silently marked `confirmed`) — see Task 5.
- `npm run typecheck` and `npm run build` must both pass with 0 errors before any task is marked done.

---

### Task 1: Supabase project — `payment_references` table + private `mahlanya-builds` storage bucket

**Model:** Opus (from-scratch schema + storage security design, no existing pattern in this repo to extend).

**Files:**
- Create: `web/supabase/migrations/001_payment_references.sql`

**Interfaces:**
- Produces: table `payment_references(id uuid PK, service_slug text, amount_cents integer, currency text default 'SZL', payer_name text, payer_contact text, emali_reference text, status text default 'pending' check in ('pending','confirmed','rejected'), created_at timestamptz, confirmed_at timestamptz, artifact_delivered_at timestamptz)`.
- Produces: private storage bucket `mahlanya-builds` (`public = false`).

- [ ] **Step 1: Create a new Supabase project**

In the Supabase dashboard, create a new project dedicated to MahlanyaRPG (per the cross-repo isolation decision — not shared with brt-inc/wheels-deals-eswatini/likhono-lami/maize-model). Note the Project URL and the `service_role` key from Project Settings → API — both are needed for env vars in Task 2.

- [ ] **Step 2: Write the migration**

```sql
CREATE TABLE IF NOT EXISTS payment_references (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  service_slug text NOT NULL,
  amount_cents integer NOT NULL CHECK (amount_cents > 0),
  currency text NOT NULL DEFAULT 'SZL',
  payer_name text NOT NULL,
  payer_contact text NOT NULL,
  emali_reference text NOT NULL,
  status text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'confirmed', 'rejected')),
  created_at timestamptz NOT NULL DEFAULT now(),
  confirmed_at timestamptz,
  artifact_delivered_at timestamptz
);

ALTER TABLE payment_references ENABLE ROW LEVEL SECURITY;
-- No policies are created deliberately: web/ never issues a browser-side Supabase
-- key (no NEXT_PUBLIC anon key exists in this app), so every read/write goes
-- through the server-only service-role key, which bypasses RLS by design. RLS is
-- enabled with zero policies as a fail-closed backstop — if an anon key is ever
-- accidentally exposed in a future change, it grants access to nothing.

INSERT INTO storage.buckets (id, name, public)
VALUES ('mahlanya-builds', 'mahlanya-builds', false)
ON CONFLICT (id) DO NOTHING;
-- Private bucket, no storage.objects policies added — same fail-closed reasoning
-- as above. Upload (by the operator, out-of-band) and signed-URL creation (on
-- confirm) both happen through the service-role key server-side.
```

- [ ] **Step 3: Apply the migration**

Paste the SQL from Step 2 into the new project's SQL Editor and run it (no `supabase` CLI project is linked yet in this repo, and standing one up is unjustified scope for a single migration — the SQL Editor is the pragmatic path here).
Expected: no errors. `select * from payment_references;` in the SQL Editor returns an empty result set (table exists, zero rows). Storage → Buckets shows `mahlanya-builds` with the private/lock icon.

- [ ] **Step 4: Upload the first build artifact**

Via Storage → `mahlanya-builds` in the dashboard (or the `supabase` CLI's `storage cp`), upload the current supporter/demo build zip to a path such as `releases/mahlanya-demo-v1.zip`. Record this exact path — it becomes `EMALI_BUILD_ARTIFACT_PATH` in Task 2. This is a one-time manual step per release; this plan does not build an in-app upload UI (single operator, infrequent releases — an upload UI is unjustified scope, same reasoning the master spec applies to maize-model's admin surface).

- [ ] **Step 5: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add web/supabase/migrations/001_payment_references.sql
git commit -m "feat(web): add payment_references table + private storage bucket for eMali reconciliation"
```

---

### Task 2: Dependencies, env scaffolding, and server-only Supabase client

**Model:** Sonnet.

**Files:**
- Modify: `web/package.json`
- Modify: `web/.gitignore`
- Create: `web/.env.example`
- Create: `web/lib/constants.ts`
- Create: `web/lib/supabaseAdmin.ts`

**Interfaces:**
- Produces: `getSupabaseAdmin(): SupabaseClient` (server-only, throws if env vars missing).
- Produces: constants `EMALI_NUMBER`, `SUPPORTER_TIER_PRICE_CENTS`, `SUPPORTER_TIER_CURRENCY`, `SUPPORTER_TIER_SERVICE_SLUG`, `BUILD_ARTIFACT_BUCKET`, `SIGNED_URL_TTL_SECONDS`.

- [ ] **Step 1: Add dependencies to `package.json`**

Edit `web/package.json`'s `"dependencies"` block to add:

```json
    "@supabase/supabase-js": "^2.45.0",
    "resend": "^4.0.0",
    "zod": "^3.23.0"
```

- [ ] **Step 2: Install**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm install
```
Expected: `node_modules/` populated, `package-lock.json` created/updated, no install errors.

- [ ] **Step 3: Add `.next` to `web/.gitignore`**

`web/.gitignore` currently contains only `.vercel` — the repo root `.gitignore` already covers `node_modules/` and `.env*` globally, but nothing covers Next's `.next/` build output. Edit `web/.gitignore`:

```
.vercel
.next
```

- [ ] **Step 4: Create the env example file**

Name it `.env.example` (not `.env.local.example`) so it matches the root `.gitignore`'s existing `!.env.example` negation — a `.env.local.example` would be silently swallowed by the root's broad `.env.*` ignore pattern.

```bash
cat > web/.env.example << 'EOF'
NEXT_PUBLIC_SUPABASE_URL=
SUPABASE_SERVICE_ROLE_KEY=
RESEND_API_KEY=
ADMIN_EMALI_SECRET=
EMALI_BUILD_ARTIFACT_PATH=releases/mahlanya-demo-v1.zip
EOF
```

- [ ] **Step 5: Create `web/lib/constants.ts`**

```typescript
export const EMALI_NUMBER = '+26879657744'
export const SUPPORTER_TIER_PRICE_CENTS = 5000
export const SUPPORTER_TIER_CURRENCY = 'SZL'
export const SUPPORTER_TIER_SERVICE_SLUG = 'mahlanya-demo-supporter'
export const BUILD_ARTIFACT_BUCKET = 'mahlanya-builds'
export const SIGNED_URL_TTL_SECONDS = 7 * 24 * 60 * 60
```

- [ ] **Step 6: Create `web/lib/supabaseAdmin.ts`**

```typescript
import { createClient, type SupabaseClient } from '@supabase/supabase-js'

let cached: SupabaseClient | null = null

export function getSupabaseAdmin(): SupabaseClient {
  if (cached) {
    return cached
  }
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL
  const serviceRoleKey = process.env.SUPABASE_SERVICE_ROLE_KEY
  if (!url || !serviceRoleKey) {
    throw new Error('Supabase server credentials are not configured')
  }
  cached = createClient(url, serviceRoleKey, {
    auth: { persistSession: false },
  })
  return cached
}
```

- [ ] **Step 7: Type-check**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
```
Expected: 0 errors.

- [ ] **Step 8: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add web/package.json web/package-lock.json web/.gitignore web/.env.example web/lib/constants.ts web/lib/supabaseAdmin.ts
git commit -m "feat(web): add Supabase/Resend/zod deps, env scaffolding, server-only Supabase client"
```

---

### Task 3: Public submit API route

**Model:** Sonnet.

**Files:**
- Create: `web/app/api/emali/submit/route.ts`

**Interfaces:**
- Consumes: `getSupabaseAdmin()`, `SUPPORTER_TIER_*` constants from Task 2.
- Produces: `POST /api/emali/submit` accepting `{ payerName, payerContact, emaliReference }`, returns `{ ok: true, id: string }` or `{ error: string }`.

- [ ] **Step 1: Write the route**

```typescript
import { NextRequest, NextResponse } from 'next/server'
import { z } from 'zod'
import { Resend } from 'resend'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import {
  SUPPORTER_TIER_PRICE_CENTS,
  SUPPORTER_TIER_CURRENCY,
  SUPPORTER_TIER_SERVICE_SLUG,
} from '@/lib/constants'

const RATE_MAP = new Map<string, number[]>()
const RATE_WINDOW_MS = 15 * 60 * 1000
const RATE_MAX = 5

function isRateLimited(ip: string): boolean {
  const now = Date.now()
  const prev = (RATE_MAP.get(ip) ?? []).filter((t) => now - t < RATE_WINDOW_MS)
  if (prev.length >= RATE_MAX) {
    return true
  }
  prev.push(now)
  RATE_MAP.set(ip, prev)
  return false
}

const SubmitSchema = z.object({
  payerName: z.string().min(1).max(200),
  payerContact: z.string().min(1).max(200),
  emaliReference: z.string().min(1).max(100),
})

export async function POST(request: NextRequest) {
  const ip = request.headers.get('x-forwarded-for')?.split(',')[0].trim() ?? 'unknown'
  if (isRateLimited(ip)) {
    return NextResponse.json({ error: 'Too many requests — try again in 15 minutes.' }, { status: 429 })
  }

  let body: unknown
  try {
    body = await request.json()
  } catch {
    return NextResponse.json({ error: 'Invalid JSON' }, { status: 400 })
  }

  const parsed = SubmitSchema.safeParse(body)
  if (!parsed.success) {
    return NextResponse.json({ error: 'Invalid submission' }, { status: 400 })
  }
  const { payerName, payerContact, emaliReference } = parsed.data

  const supabase = getSupabaseAdmin()
  const { data, error } = await supabase
    .from('payment_references')
    .insert({
      service_slug: SUPPORTER_TIER_SERVICE_SLUG,
      amount_cents: SUPPORTER_TIER_PRICE_CENTS,
      currency: SUPPORTER_TIER_CURRENCY,
      payer_name: payerName,
      payer_contact: payerContact,
      emali_reference: emaliReference,
    })
    .select('id')
    .single()

  if (error || !data) {
    return NextResponse.json({ error: 'Could not record submission' }, { status: 500 })
  }

  const resend = new Resend(process.env.RESEND_API_KEY)
  try {
    await resend.emails.send({
      from: 'Mahlanya <onboarding@resend.dev>',
      to: 'charleskris9@gmail.com',
      subject: `New eMali reference — ${payerName}`,
      html: `<p>${payerName} (${payerContact}) submitted eMali reference <strong>${emaliReference}</strong> for the supporter build (E${(SUPPORTER_TIER_PRICE_CENTS / 100).toFixed(2)}). Confirm at /admin/emali.</p>`,
    })
  } catch {
    // Notification failure must not block the recorded submission — the admin list is authoritative.
  }

  return NextResponse.json({ ok: true, id: data.id })
}
```

- [ ] **Step 2: Type-check**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
```
Expected: 0 errors.

- [ ] **Step 3: Manual verification against the dev server**

Populate `web/.env.local` (gitignored) with real values from Task 1's project, then:
```bash
cd ~/my-projects/MahlanyaRPG/web
npm run dev
```
In another shell:
```bash
curl -s -X POST http://localhost:3001/api/emali/submit \
  -H 'Content-Type: application/json' \
  -d '{"payerName":"Test User","payerContact":"+26876000000","emaliReference":"TESTREF123"}'
```
Expected: `{"ok":true,"id":"<uuid>"}`. Confirm the row appears in the Supabase table editor with `status = 'pending'` and `amount_cents = 5000` (the server-side constant, not client-supplied).

- [ ] **Step 4: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add web/app/api/emali/submit/route.ts
git commit -m "feat(web): add public eMali payment reference submission endpoint"
```

---

### Task 4: Access page + submission form UI

**Model:** Sonnet.

**Files:**
- Create: `web/app/access/page.tsx`
- Create: `web/components/EmaliPaymentForm.tsx`
- Modify: `web/app/page.tsx`

**Interfaces:**
- Consumes: `POST /api/emali/submit` from Task 3; `EMALI_NUMBER`, `SUPPORTER_TIER_PRICE_CENTS`, `SUPPORTER_TIER_CURRENCY` from Task 2.

- [ ] **Step 1: Write the submission form component**

```tsx
'use client'

import { useState } from 'react'

export default function EmaliPaymentForm() {
  const [status, setStatus] = useState<'idle' | 'submitting' | 'done' | 'error'>('idle')
  const [errorMessage, setErrorMessage] = useState('')

  async function handleSubmit(event: React.FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setStatus('submitting')
    setErrorMessage('')
    const form = new FormData(event.currentTarget)
    try {
      const res = await fetch('/api/emali/submit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          payerName: String(form.get('name')),
          payerContact: String(form.get('contact')),
          emaliReference: String(form.get('reference')),
        }),
      })
      if (!res.ok) {
        const body = await res.json().catch(() => ({ error: 'Submission failed' }))
        setErrorMessage(body.error ?? 'Submission failed')
        setStatus('error')
        return
      }
      setStatus('done')
    } catch {
      setErrorMessage('Network error — try again')
      setStatus('error')
    }
  }

  if (status === 'done') {
    return <p>Reference received &mdash; we&apos;ll confirm and email your download link within 7 days.</p>
  }

  const inputStyle = { width: '100%', padding: 8, marginTop: 4, marginBottom: 12 }

  return (
    <form onSubmit={handleSubmit} style={{ marginTop: 24 }}>
      <label>
        Your name
        <input name="name" required style={inputStyle} />
      </label>
      <label>
        Contact email or phone
        <input name="contact" required style={inputStyle} />
      </label>
      <label>
        eMali transaction reference
        <input name="reference" required style={inputStyle} />
      </label>
      <button type="submit" disabled={status === 'submitting'} style={{ padding: '8px 16px' }}>
        {status === 'submitting' ? 'Submitting…' : 'Submit reference'}
      </button>
      {status === 'error' && <p style={{ color: '#e66' }}>{errorMessage}</p>}
    </form>
  )
}
```

- [ ] **Step 2: Write the access page**

```tsx
import EmaliPaymentForm from '../../components/EmaliPaymentForm'
import { EMALI_NUMBER, SUPPORTER_TIER_PRICE_CENTS, SUPPORTER_TIER_CURRENCY } from '../../lib/constants'

export default function AccessPage() {
  return (
    <main
      style={{
        maxWidth: 640,
        margin: '0 auto',
        padding: '48px 24px',
        fontFamily: 'system-ui, sans-serif',
        color: '#eaeaea',
        background: '#0a0a0a',
        minHeight: '100vh',
      }}
    >
      <h1>Get the Mahlanya supporter build</h1>
      <p>
        Support development and get early access to the current playable build. Pay{' '}
        {(SUPPORTER_TIER_PRICE_CENTS / 100).toFixed(2)} {SUPPORTER_TIER_CURRENCY} to{' '}
        <strong>{EMALI_NUMBER}</strong> via the eMali app, then submit your details and the
        transaction reference below.
      </p>
      <p>
        References are confirmed manually &mdash; this is not instant. Once confirmed, a
        download link (valid for 7 days) is emailed to the contact you provide.
      </p>
      <EmaliPaymentForm />
    </main>
  )
}
```

- [ ] **Step 3: Link the access page from the viewer**

Read the current `web/app/page.tsx` (already read during planning — it is exactly 9 lines, no existing links) and replace it with:

```tsx
import Link from 'next/link';
import HeightmapViewer from '../components/HeightmapViewer';

export default function Page() {
  return (
    <main style={{ width: '100vw', height: '100vh', background: '#0a0a0a', position: 'relative' }}>
      <HeightmapViewer />
      <Link
        href="/access"
        style={{
          position: 'absolute',
          top: 12,
          right: 12,
          padding: '8px 12px',
          background: 'rgba(0,0,0,0.55)',
          color: '#eaeaea',
          font: '13px system-ui, sans-serif',
          borderRadius: 4,
          textDecoration: 'none',
          zIndex: 10,
        }}
      >
        Get supporter build &rarr;
      </Link>
    </main>
  );
}
```

- [ ] **Step 4: Type-check and manual browser check**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
npm run build
npm run dev
```
Load `http://localhost:3001` — confirm the "Get supporter build" link appears top-right over the viewer. Click it, confirm `/access` renders the price/instructions/form. Submit a test reference and confirm the "Reference received" message appears (reuses the Task 3 verification path).

- [ ] **Step 5: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add web/app/access/page.tsx web/components/EmaliPaymentForm.tsx web/app/page.tsx
git commit -m "feat(web): add eMali access page and link it from the viewer"
```

---

### Task 5: Admin auth, list, and confirm/reject with signed-URL delivery

**Model:** Opus (security-boundary design: shared-secret auth, signed-URL minting, fail-closed delivery — no existing pattern in this repo to extend).

**Files:**
- Create: `web/lib/adminAuth.ts`
- Create: `web/app/api/emali/list/route.ts`
- Create: `web/app/api/emali/[id]/route.ts`

**Interfaces:**
- Consumes: `payment_references` table and `mahlanya-builds` bucket from Task 1; `getSupabaseAdmin()`, `BUILD_ARTIFACT_BUCKET`, `SIGNED_URL_TTL_SECONDS` from Task 2.
- Produces: `isAuthorizedAdmin(request: Request): boolean`.
- Produces: `GET /api/emali/list` → `{ references: PaymentReference[] }` or 401.
- Produces: `PATCH /api/emali/[id]` accepting `{ action: 'confirm' | 'reject' }` with header `Authorization: Bearer <ADMIN_EMALI_SECRET>` → `{ ok: true }` or 401/404/400/500/502.

- [ ] **Step 1: Write the shared-secret auth helper**

```typescript
import { timingSafeEqual } from 'crypto'

export function isAuthorizedAdmin(request: Request): boolean {
  const authHeader = request.headers.get('authorization') ?? ''
  const provided = authHeader.startsWith('Bearer ') ? authHeader.slice(7).trim() : ''
  const expected = process.env.ADMIN_EMALI_SECRET ?? ''
  if (!provided || !expected) {
    return false
  }
  const providedBuf = Buffer.from(provided)
  const expectedBuf = Buffer.from(expected)
  if (providedBuf.length !== expectedBuf.length) {
    return false
  }
  return timingSafeEqual(providedBuf, expectedBuf)
}
```

Fails closed on both empty-secret cases (unset env var, or missing header) before ever calling `timingSafeEqual` — that function throws on mismatched buffer lengths, so the length check must happen first.

- [ ] **Step 2: Write the list route**

```typescript
import { NextRequest, NextResponse } from 'next/server'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import { isAuthorizedAdmin } from '@/lib/adminAuth'

export async function GET(request: NextRequest) {
  if (!isAuthorizedAdmin(request)) {
    return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })
  }

  const supabase = getSupabaseAdmin()
  const { data, error } = await supabase
    .from('payment_references')
    .select('*')
    .order('created_at', { ascending: false })

  if (error) {
    return NextResponse.json({ error: 'Could not load references' }, { status: 500 })
  }

  return NextResponse.json({ references: data })
}
```

- [ ] **Step 3: Write the confirm/reject route**

Next.js 14.2 App Router dynamic route params are synchronous objects (not promises — that changed in Next 15), so the handler signature below uses `{ params: { id: string } }`, not the promise form.

```typescript
import { NextRequest, NextResponse } from 'next/server'
import { z } from 'zod'
import { Resend } from 'resend'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import { isAuthorizedAdmin } from '@/lib/adminAuth'
import { BUILD_ARTIFACT_BUCKET, SIGNED_URL_TTL_SECONDS } from '@/lib/constants'

const ActionSchema = z.object({ action: z.enum(['confirm', 'reject']) })

export async function PATCH(request: NextRequest, { params }: { params: { id: string } }) {
  if (!isAuthorizedAdmin(request)) {
    return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })
  }

  let body: unknown
  try {
    body = await request.json()
  } catch {
    return NextResponse.json({ error: 'Invalid JSON' }, { status: 400 })
  }
  const parsed = ActionSchema.safeParse(body)
  if (!parsed.success) {
    return NextResponse.json({ error: 'Invalid action' }, { status: 400 })
  }

  const supabase = getSupabaseAdmin()
  const { data: existing, error: fetchError } = await supabase
    .from('payment_references')
    .select('*')
    .eq('id', params.id)
    .eq('status', 'pending')
    .single()

  if (fetchError || !existing) {
    return NextResponse.json({ error: 'Reference not found or already resolved' }, { status: 404 })
  }

  if (parsed.data.action === 'reject') {
    const { error } = await supabase
      .from('payment_references')
      .update({ status: 'rejected', confirmed_at: new Date().toISOString() })
      .eq('id', params.id)
      .eq('status', 'pending')
    if (error) {
      return NextResponse.json({ error: 'Could not update reference' }, { status: 500 })
    }
    return NextResponse.json({ ok: true })
  }

  const artifactPath = process.env.EMALI_BUILD_ARTIFACT_PATH
  if (!artifactPath) {
    return NextResponse.json({ error: 'Build artifact path not configured' }, { status: 500 })
  }

  const { data: signed, error: signError } = await supabase.storage
    .from(BUILD_ARTIFACT_BUCKET)
    .createSignedUrl(artifactPath, SIGNED_URL_TTL_SECONDS)

  if (signError || !signed) {
    return NextResponse.json({ error: 'Could not generate download link' }, { status: 500 })
  }

  const resend = new Resend(process.env.RESEND_API_KEY)
  try {
    await resend.emails.send({
      from: 'Mahlanya <onboarding@resend.dev>',
      to: existing.payer_contact,
      subject: 'Your Mahlanya supporter build download',
      html: `<p>Thanks for supporting Mahlanya, ${existing.payer_name}. Your download link is valid for 7 days:</p><p><a href="${signed.signedUrl}">${signed.signedUrl}</a></p>`,
    })
  } catch {
    return NextResponse.json(
      { error: 'Payment confirmed but delivery email failed — retry from the admin page' },
      { status: 502 },
    )
  }

  const { error: updateError } = await supabase
    .from('payment_references')
    .update({
      status: 'confirmed',
      confirmed_at: new Date().toISOString(),
      artifact_delivered_at: new Date().toISOString(),
    })
    .eq('id', params.id)
    .eq('status', 'pending')

  if (updateError) {
    return NextResponse.json({ error: 'Email sent but status update failed — check the table manually' }, { status: 500 })
  }

  return NextResponse.json({ ok: true })
}
```

The row is deliberately left `pending` (not flipped to `confirmed`) if the Resend send throws — a "confirmed but never delivered" state must never be silently reachable. The `.eq('status', 'pending')` guard on every write prevents a double-confirm race from firing two delivery emails for one reference.

- [ ] **Step 4: Type-check**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
```
Expected: 0 errors.

- [ ] **Step 5: Manual verification**

With `ADMIN_EMALI_SECRET` set in `web/.env.local` and the dev server running, using the `pending` row's id from Task 3's test:
```bash
curl -s http://localhost:3001/api/emali/list \
  -H "Authorization: Bearer $ADMIN_EMALI_SECRET"
```
Expected: `{"references":[{...,"status":"pending",...}]}`.

```bash
curl -s -X PATCH http://localhost:3001/api/emali/<id> \
  -H "Authorization: Bearer $ADMIN_EMALI_SECRET" \
  -H 'Content-Type: application/json' \
  -d '{"action":"confirm"}'
```
Expected: `{"ok":true}`. Confirm the row's `status` is now `confirmed`, `artifact_delivered_at` is set, and (if `RESEND_API_KEY` is a real key) the test email arrives at the `payer_contact` address with a working signed URL. Also verify with a wrong or missing bearer token that both routes return 401.

- [ ] **Step 6: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add web/lib/adminAuth.ts web/app/api/emali/list/route.ts "web/app/api/emali/[id]/route.ts"
git commit -m "feat(web): add admin auth + confirm/reject endpoint with 7-day signed-URL delivery"
```

---

### Task 6: Admin confirm page UI

**Model:** Sonnet (consumes the Task 5 API against a complete spec).

**Files:**
- Create: `web/app/admin/emali/page.tsx`

**Interfaces:**
- Consumes: `GET /api/emali/list` and `PATCH /api/emali/[id]` from Task 5.

- [ ] **Step 1: Write the page**

```tsx
'use client'

import { useState } from 'react'

type PaymentReference = {
  id: string
  service_slug: string
  amount_cents: number
  currency: string
  payer_name: string
  payer_contact: string
  emali_reference: string
  status: 'pending' | 'confirmed' | 'rejected'
  created_at: string
}

export default function EmaliAdminPage() {
  const [secret, setSecret] = useState('')
  const [unlocked, setUnlocked] = useState(false)
  const [refs, setRefs] = useState<PaymentReference[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')

  async function load(currentSecret: string) {
    setLoading(true)
    setError('')
    const res = await fetch('/api/emali/list', {
      headers: { Authorization: `Bearer ${currentSecret}` },
    })
    if (!res.ok) {
      setError('Unauthorized or request failed')
      setUnlocked(false)
      setLoading(false)
      return
    }
    const body = await res.json()
    setRefs(body.references)
    setUnlocked(true)
    setLoading(false)
  }

  async function act(id: string, action: 'confirm' | 'reject') {
    const res = await fetch(`/api/emali/${id}`, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${secret}`,
      },
      body: JSON.stringify({ action }),
    })
    if (!res.ok) {
      const responseBody = await res.json().catch(() => ({ error: 'Unknown error' }))
      setError(responseBody.error ?? 'Action failed')
      return
    }
    load(secret)
  }

  if (!unlocked) {
    return (
      <main style={{ maxWidth: 480, margin: '80px auto', padding: 24, fontFamily: 'system-ui, sans-serif' }}>
        <h1>eMali admin</h1>
        <input
          type="password"
          value={secret}
          onChange={(e) => setSecret(e.target.value)}
          placeholder="Admin secret"
          style={{ width: '100%', padding: 8, marginTop: 12 }}
        />
        <button onClick={() => load(secret)} disabled={loading || !secret} style={{ marginTop: 12, padding: '8px 16px' }}>
          {loading ? 'Checking…' : 'Unlock'}
        </button>
        {error && <p style={{ color: '#c33' }}>{error}</p>}
      </main>
    )
  }

  return (
    <main style={{ maxWidth: 720, margin: '40px auto', padding: 24, fontFamily: 'system-ui, sans-serif' }}>
      <h1>eMali payment references</h1>
      {error && <p style={{ color: '#c33' }}>{error}</p>}
      {refs.map((r) => (
        <div key={r.id} style={{ border: '1px solid #333', borderRadius: 4, padding: 16, marginTop: 12 }}>
          <p>{r.payer_name} ({r.payer_contact}) &mdash; E{(r.amount_cents / 100).toFixed(2)} {r.currency}</p>
          <p>Ref: {r.emali_reference} &mdash; Status: {r.status}</p>
          {r.status === 'pending' && (
            <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
              <button onClick={() => act(r.id, 'confirm')}>Confirm</button>
              <button onClick={() => act(r.id, 'reject')}>Reject</button>
            </div>
          )}
        </div>
      ))}
    </main>
  )
}
```

The secret is held only in React state (never `localStorage`/`sessionStorage`) and re-entered each session — it is operator input transmitted over HTTPS, not a value baked into the JS bundle, so it does not fall under the "no client-bundle secrets" concern that applies to build-time `NEXT_PUBLIC_` env vars.

- [ ] **Step 2: Type-check and manual browser check**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
npm run build
npm run dev
```
Visit `http://localhost:3001/admin/emali`. Enter the wrong secret — confirm "Unauthorized or request failed" and the list stays locked. Enter the correct `ADMIN_EMALI_SECRET` — confirm the list loads, showing the reference from Task 5's test as `confirmed`. Submit a fresh test reference via `/access`, reload the admin page, and confirm the new row appears `pending` with working Confirm/Reject buttons.

- [ ] **Step 3: Commit**

```bash
cd ~/my-projects/MahlanyaRPG
git add "web/app/admin/emali/page.tsx"
git commit -m "feat(web): add eMali admin confirm/reject page"
```

---

### Task 7: Blocking checks, Vercel env vars, deploy

**Model:** Sonnet (mechanical verification and deploy wiring — no design judgment left).

- [ ] **Step 1: Confirm the 316-test / historical-accuracy gates are untouched**

```bash
cd ~/my-projects/MahlanyaRPG
git diff --stat origin/master -- pipeline/ Content/ Source/
```
Expected: empty output — this plan added files only under `web/`, so the `pipeline-ci.yml` and `content-validate.yml` path-triggers never fire for this branch.

- [ ] **Step 2: Full typecheck and build**

```bash
cd ~/my-projects/MahlanyaRPG/web
npm run typecheck
npm run build
```
Expected: 0 type errors, build succeeds, no missing imports.

- [ ] **Step 3: Set Vercel environment variables**

```bash
cd ~/my-projects/MahlanyaRPG/web
vercel env add NEXT_PUBLIC_SUPABASE_URL production
vercel env add SUPABASE_SERVICE_ROLE_KEY production
vercel env add RESEND_API_KEY production
vercel env add ADMIN_EMALI_SECRET production
vercel env add EMALI_BUILD_ARTIFACT_PATH production
```
Expected: each prompts for a value and confirms it was added to the `mahlanya-world-viewer` project (per `.vercel/project.json`). Repeat for `preview`/`development` environments if preview deploys should also exercise this flow.

- [ ] **Step 4: Push and confirm deploy**

```bash
cd ~/my-projects/MahlanyaRPG
git push origin master
```
Expected: Vercel auto-deploys via its GitHub integration (per the linked `.vercel/project.json`). Confirm the deploy succeeds in the Vercel dashboard, then load the production `/access` and `/admin/emali` URLs to confirm they render before considering this plan complete.

---

## Self-Review

**Spec coverage** — every bullet in `docs/superpowers/specs/2026-08-09-emali-monetization-design.md` maps to a task: isolated Supabase project + table + bucket → Task 1; deps/env/client → Task 2; public submit → Task 3; access page → Task 4; admin auth/list/confirm/signed-URL/delivery → Task 5; admin UI → Task 6; gates/deploy → Task 7. No spec bullet is unaddressed.

**Placeholder scan** — no "TBD"/"handle appropriately"/"similar to Task N" language; every step has real, complete code. The two genuinely undetermined values (`SUPPORTER_TIER_PRICE_CENTS`, the Resend sending domain) are not disguised as decided — they're called out explicitly as operator follow-ups in both the spec and Task 2/3, not silently assumed.

**Type consistency** — `PaymentReference` shape (`id, service_slug, amount_cents, currency, payer_name, payer_contact, emali_reference, status, created_at`) is used identically in Task 5's `GET /api/emali/list` and Task 6's admin page. `isAuthorizedAdmin(request: Request): boolean` (Task 5, Step 1) is imported with that exact signature in both Task 5's `list`/`[id]` routes. `getSupabaseAdmin(): SupabaseClient` (Task 2) is imported identically in Tasks 3 and 5. Constant names (`SUPPORTER_TIER_PRICE_CENTS`, `SUPPORTER_TIER_CURRENCY`, `SUPPORTER_TIER_SERVICE_SLUG`, `BUILD_ARTIFACT_BUCKET`, `SIGNED_URL_TTL_SECONDS`, `EMALI_NUMBER`) are defined once in Task 2 and referenced by the same names in Tasks 3, 4, and 5 — no drift.

**Next.js version check** — `web/package.json` pins `"next": "14.2.0"`. Task 5's dynamic route handler uses the Next 14 synchronous `{ params: { id: string } }` signature, not the Next 15+ promise form used by brt-inc's Next 16 plan — called out explicitly in Task 5 so an implementer skimming the sibling brt-inc plan doesn't copy the wrong signature.
