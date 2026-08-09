# eMali Manual Payment — MahlanyaRPG (`web/`) Design

Date: 2026-08-09
Scope: `web/` only (the standalone Next.js 14 companion viewer app, port 3001). Does not touch `Source/`, `pipeline/`, `Content/`, or any UE5 asset.
Sibling repos: see the cross-repo spec at `~/my-projects/personal/CBahtaria/brt-inc/docs/superpowers/specs/2026-08-09-emali-monetization-design.md` for the shared pattern and how this repo's slice fits alongside brt-inc, wheels-deals-eswatini, likhono-lami, and maize-model.

## Why this shape

Swazi Mobile E-Mali has no public merchant/developer API. The only reliable integration is manual reconciliation: a fan pays a fixed amount to `+26879657744` via the eMali app themselves, submits a claim (contact + transaction reference) through a new page, and the site owner (`charleskris9@gmail.com`) manually confirms it. Nothing unlocks on submission — only on confirmation.

`web/` currently has zero backend: no `.env`, no forms, no API routes, no database, one page (`app/page.tsx`) rendering a Three.js heightmap viewer. This is the biggest lift of the five repos — everything here is new.

## What ships

1. **New isolated Supabase project** — not shared with brt-inc, wheels-deals-eswatini, likhono-lami, or maize-model. A leaked key here can't expose any other repo's rows. This app never ships a browser-side Supabase key (no `NEXT_PUBLIC` anon key at all) — every read/write goes through a server-only service-role key inside API routes. Simpler than the sibling repos that have browser sessions, and it means Row Level Security policies aren't load-bearing here (they're a fail-closed backstop in case a key is ever exposed by mistake, not the access-control mechanism).
2. **`payment_references` table** — one row per submitted claim. `status` starts `pending`; only the authenticated confirm route can move it to `confirmed` or `rejected`.
3. **Fixed-price supporter tier** — unlike brt-inc (variable per-service pricing, client-supplied amount), MahlanyaRPG sells one thing (the supporter/demo build) at one price. The amount is a server-side constant, never read from the client request body. This removes price-tampering as a bug class entirely rather than validating against it.
4. **`web/app/access/page.tsx`** — public page: price, eMali number, instructions, and the reference-submission form.
5. **`POST /api/emali/submit`** — public, rate-limited, zod-validated. Inserts a `pending` row, emails the owner via Resend. Never auto-confirms.
6. **Private Supabase Storage bucket `mahlanya-builds`** — holds the actual build artifact (zip). The owner uploads the artifact once, out-of-band, via the Supabase dashboard or CLI — this plan does not build an in-app upload UI. That's deliberate: one operator, low volume, infrequent releases. Building upload management here is the same category of over-scoping the master spec explicitly rejects for maize-model's admin surface (an endpoint, not a UI, at this volume).
7. **Admin confirm/reject** — `GET /api/emali/list` and `PATCH /api/emali/[id]`, gated by a shared-secret `Authorization: Bearer <ADMIN_EMALI_SECRET>` header, constant-time compared. `web/` has no existing auth/session system at all (unlike brt-inc's Supabase-session-gated portal), so standing up a full login system for a single operator is disproportionate. This mirrors wheels-deals-eswatini's existing `ADMIN_SECRET` pattern referenced in the cross-repo spec, applied here from scratch since nothing existed to reuse.
8. **On confirm**: server generates a fresh 7-day Supabase Storage signed URL for the (already-uploaded) artifact object and emails it to `payer_contact` via Resend. The signed URL itself is never persisted to the database — only an `artifact_delivered_at` timestamp is stored — so a database leak cannot leak a live download link. If the delivery email fails, the row stays `pending` (does not flip to `confirmed`) so the operator can retry; a "confirmed but never delivered" state is treated as a bug, not an acceptable edge case, matching this codebase's "fail-safe means NO_GO" convention for exceptional paths.
9. **No public/permanent download link anywhere.** Every grant is a fresh, per-payer, 7-day-expiring signed URL minted at confirm time.

## Environment variables (server-only unless noted)

- `NEXT_PUBLIC_SUPABASE_URL` — project URL. Safe to expose (it's just an endpoint, not a credential).
- `SUPABASE_SERVICE_ROLE_KEY` — server-only. Never referenced from a client component, never `NEXT_PUBLIC_`-prefixed.
- `RESEND_API_KEY` — server-only.
- `ADMIN_EMALI_SECRET` — server-only. Compared with `crypto.timingSafeEqual`, fail-closed if unset or mismatched (never a default-allow).
- `EMALI_BUILD_ARTIFACT_PATH` — server-only. Object path inside `mahlanya-builds`, e.g. `releases/mahlanya-demo-v1.zip`. Changes each time the operator uploads a new build; not hardcoded, since the version will move.

**Open item flagged, not assumed:** Resend requires a verified sending domain for production `from` addresses. This repo has no evidence of one configured (no `.env`, no prior email sends). The plan uses Resend's sandbox `onboarding@resend.dev` sender for dev/testing; swapping to a verified domain is an operator task outside this plan's scope, called out here so it isn't silently forgotten.

**Pricing placeholder flagged, not assumed:** the master spec says "a fan pays a fixed amount" but doesn't state the amount. The plan uses `SUPPORTER_TIER_PRICE_CENTS = 5000` (E50.00) as a single named constant the operator edits before going live — not a real, confirmed price.

## Global Constraints — parent CLAUDE.md applicability

`~/my-projects/MahlanyaRPG/CLAUDE.md` governs the UE5/Zig/Python game codebase. `web/` is a separate Next.js companion app with its own `package.json`, no build dependency on `Source/`, `pipeline/`, or `Content/`. Verified against the actual CI wiring, not assumed:

- `.github/workflows/pipeline-ci.yml` (the 316-test pytest/Zig suite) triggers only on `pipeline/**` paths.
- `.github/workflows/content-validate.yml` (the historical-accuracy gate, `pipeline/history/validate_content.py`) triggers only on `Content/Dialogue/**` and `pipeline/history/data/**` paths.
- `.github/workflows/phase10-validation.yml` triggers only on `Source/MahlanyaRPG/Performance/**` and `Source/MahlanyaRPG/Core/MahlanyaLogChannels.*`.
- `.github/workflows/ue5-build.yml` triggers only on `Source/**`, `Plugins/**`, `Config/**`, `*.uproject`.
- No workflow anywhere references `web/`.
- The specific script paths named in `CLAUDE.md` ("What done looks like" — `scripts/pipeline_full.py`, `scripts/historical_accuracy_check.py`, `scripts/performance_regression.py`) do not exist under those names anywhere in the repo; the actual enforced gates are the workflow files above, whose real script paths (`pipeline/history/validate_content.py`, `pipeline/tests/`) are also path-scoped away from `web/`. This is a pre-existing doc/reality mismatch in `CLAUDE.md`, noted here as a finding, not fixed by this plan (out of scope).

Conclusion: none of the 8 hard lines apply to this task. See the plan's Global Constraints section for the line-by-line reasoning.

## Model routing for this work

Per `CLAUDE.md`: "Sonnet 4.6 for UI, dialogue, quest scripting, and asset pipeline work" — `web/` is UI/companion-app work, so Sonnet is the default for every task. Two tasks are escalated to Opus as a judgment call, not because the routing table names them: the from-scratch Supabase schema + private storage bucket design (Task 1), and the admin auth + signed-URL generation + fail-closed delivery logic (Task 5) — both are security-boundary decisions being made with no existing pattern in this repo to copy, unlike e.g. brt-inc where an auth boundary already existed to extend.
