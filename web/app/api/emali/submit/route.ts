import { NextRequest, NextResponse } from 'next/server'
import { z } from 'zod'
import { Resend } from 'resend'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import { escapeHtml } from '@/lib/escapeHtml'
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
    const safeName = escapeHtml(payerName)
    const safeContact = escapeHtml(payerContact)
    const safeReference = escapeHtml(emaliReference)
    await resend.emails.send({
      from: 'Mahlanya RPG <mahlanya@brtinc.dev>',
      to: 'charleskris9@gmail.com',
      subject: `New eMali reference — ${payerName}`,
      html: `<p>${safeName} (${safeContact}) submitted eMali reference <strong>${safeReference}</strong> for the supporter build (E${(SUPPORTER_TIER_PRICE_CENTS / 100).toFixed(2)}). Confirm at /admin/emali.</p>`,
    })
  } catch {
    // Notification failure must not block the recorded submission — the admin list is authoritative.
  }

  return NextResponse.json({ ok: true, id: data.id })
}
