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
  // Per-row in-flight guard. Without this a double-click fires two concurrent
  // PATCHes; both pass the API's `status = 'pending'` check before either
  // write lands, and since confirm sends the delivery email before marking
  // the row non-pending, both could email the payer before the loser 404s.
  const [busyIds, setBusyIds] = useState<Set<string>>(new Set())

  async function load(currentSecret: string) {
    setLoading(true)
    setError('')
    try {
      const res = await fetch('/api/emali/list', {
        headers: { Authorization: `Bearer ${currentSecret}` },
      })
      if (!res.ok) {
        setError('Unauthorized or request failed')
        setUnlocked(false)
        return
      }
      const body = await res.json()
      setRefs(body.references ?? [])
      setUnlocked(true)
    } catch {
      setError('Network error — try again')
      setUnlocked(false)
    } finally {
      setLoading(false)
    }
  }

  // Branches on res.ok (HTTP status) only — never on error message text. The
  // server's `error` field is shown verbatim because it carries real signal:
  // confirm-path 404 means a delivery email already went out for a request
  // whose write lost a race; 502 means the row is still `pending` and nothing
  // was sent, safe to retry. Both are distinguishable to the operator only if
  // we surface the real string instead of writing our own generic one.
  async function act(id: string, action: 'confirm' | 'reject') {
    if (busyIds.has(id)) {
      return
    }
    setBusyIds((prev) => new Set(prev).add(id))
    setError('')
    try {
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
      await load(secret)
    } catch {
      setError('Network error — try again')
    } finally {
      setBusyIds((prev) => {
        const next = new Set(prev)
        next.delete(id)
        return next
      })
    }
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
              <button onClick={() => act(r.id, 'confirm')} disabled={busyIds.has(r.id)}>
                {busyIds.has(r.id) ? 'Working…' : 'Confirm'}
              </button>
              <button onClick={() => act(r.id, 'reject')} disabled={busyIds.has(r.id)}>
                {busyIds.has(r.id) ? 'Working…' : 'Reject'}
              </button>
            </div>
          )}
        </div>
      ))}
    </main>
  )
}
