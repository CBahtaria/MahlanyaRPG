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
