import { NextRequest, NextResponse } from 'next/server'
import { z } from 'zod'
import { Resend } from 'resend'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import { isAuthorizedAdmin } from '@/lib/adminAuth'
import { escapeHtml } from '@/lib/escapeHtml'
import { BUILD_ARTIFACT_BUCKET, SIGNED_URL_TTL_SECONDS } from '@/lib/constants'

export const dynamic = 'force-dynamic'

const ActionSchema = z.object({ action: z.enum(['confirm', 'reject']) })

// Next.js 14.2 App Router dynamic params are a synchronous object, not a promise.
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
    // .select() is required to learn the row count: supabase-js reports error:null
    // for a filtered UPDATE that matched zero rows, so without it a no-op write
    // would report success.
    const { data: rejected, error } = await supabase
      .from('payment_references')
      .update({ status: 'rejected', confirmed_at: new Date().toISOString() })
      .eq('id', params.id)
      .eq('status', 'pending')
      .select('id')
    if (error) {
      return NextResponse.json({ error: 'Could not update reference' }, { status: 500 })
    }
    if (!rejected || rejected.length === 0) {
      return NextResponse.json({ error: 'Reference not found or already resolved' }, { status: 404 })
    }
    return NextResponse.json({ ok: true })
  }

  const artifactPath = process.env.EMALI_BUILD_ARTIFACT_PATH
  if (!artifactPath) {
    return NextResponse.json({ error: 'Build artifact path not configured' }, { status: 500 })
  }

  // The Resend constructor throws synchronously on a falsy key. Check it here so a
  // misconfigured deployment returns a structured error instead of an uncaught 500.
  if (!process.env.RESEND_API_KEY) {
    return NextResponse.json({ error: 'Email delivery not configured' }, { status: 500 })
  }

  const { data: signed, error: signError } = await supabase.storage
    .from(BUILD_ARTIFACT_BUCKET)
    .createSignedUrl(artifactPath, SIGNED_URL_TTL_SECONDS)

  if (signError || !signed) {
    return NextResponse.json({ error: 'Could not generate download link' }, { status: 500 })
  }

  // Resend converts non-2xx responses, JSON parse failures, and network errors alike
  // into a returned `error` field — it does not throw. The returned error is therefore
  // the load-bearing check; the try/catch is only defense-in-depth against a future
  // SDK version that does throw. Either way the row stays `pending`, because a
  // "confirmed but never delivered" state must not be silently reachable.
  let sendFailed = false
  try {
    const safeName = escapeHtml(String(existing.payer_name))
    const safeUrl = escapeHtml(signed.signedUrl)
    const resend = new Resend(process.env.RESEND_API_KEY)
    const { error: sendError } = await resend.emails.send({
      from: 'Mahlanya RPG <mahlanya@brtinc.dev>',
      to: String(existing.payer_contact),
      subject: 'Your Mahlanya supporter build download',
      html: `<p>Thanks for supporting Mahlanya, ${safeName}. Your download link is valid for 7 days:</p><p><a href="${safeUrl}">${safeUrl}</a></p>`,
    })
    sendFailed = Boolean(sendError)
  } catch {
    sendFailed = true
  }

  if (sendFailed) {
    return NextResponse.json(
      { error: 'Payment not confirmed — delivery email failed, retry from the admin page' },
      { status: 502 },
    )
  }

  const { data: confirmed, error: updateError } = await supabase
    .from('payment_references')
    .update({
      status: 'confirmed',
      confirmed_at: new Date().toISOString(),
      artifact_delivered_at: new Date().toISOString(),
    })
    .eq('id', params.id)
    .eq('status', 'pending')
    .select('id')

  if (updateError) {
    return NextResponse.json(
      { error: 'Email sent but status update failed — check the table manually' },
      { status: 500 },
    )
  }
  if (!confirmed || confirmed.length === 0) {
    return NextResponse.json(
      { error: 'Reference not found or already resolved — a delivery email was sent, check for a duplicate' },
      { status: 404 },
    )
  }

  return NextResponse.json({ ok: true })
}
