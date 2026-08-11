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
        Email
        <input name="contact" type="email" required style={inputStyle} />
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
