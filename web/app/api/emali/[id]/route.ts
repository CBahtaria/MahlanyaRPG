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
    const safeName = escapeHtml(String(existing.payer_name))
    const safeUrl = escapeHtml(signed.signedUrl)
    await resend.emails.send({
      from: 'Mahlanya RPG <mahlanya@brtinc.dev>',
      to: String(existing.payer_contact),
      subject: 'Your Mahlanya supporter build download',
      html: `<p>Thanks for supporting Mahlanya, ${safeName}. Your download link is valid for 7 days:</p><p><a href="${safeUrl}">${safeUrl}</a></p>`,
    })
  } catch {
    // Row stays `pending` deliberately: a "confirmed but never delivered" state
    // must never be silently reachable. Retry from the admin page.
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
    return NextResponse.json(
      { error: 'Email sent but status update failed — check the table manually' },
      { status: 500 },
    )
  }

  return NextResponse.json({ ok: true })
}
