import { NextRequest, NextResponse } from 'next/server'
import { getSupabaseAdmin } from '@/lib/supabaseAdmin'
import { isAuthorizedAdmin } from '@/lib/adminAuth'

// Never prerendered: the auth decision and the query both depend on the request.
export const dynamic = 'force-dynamic'

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
