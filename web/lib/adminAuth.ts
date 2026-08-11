import { timingSafeEqual } from 'crypto'

// Shared-secret admin gate. web/ has no session/auth system at all, so the
// confirm endpoint is guarded by a single operator-held bearer token.
export function isAuthorizedAdmin(request: Request): boolean {
  const authHeader = request.headers.get('authorization') ?? ''
  const provided = authHeader.startsWith('Bearer ') ? authHeader.slice(7).trim() : ''
  const expected = process.env.ADMIN_EMALI_SECRET ?? ''
  if (!provided || !expected) {
    return false
  }
  const providedBuf = Buffer.from(provided)
  const expectedBuf = Buffer.from(expected)
  // timingSafeEqual throws on unequal lengths, so this check must precede it.
  if (providedBuf.length !== expectedBuf.length) {
    return false
  }
  return timingSafeEqual(providedBuf, expectedBuf)
}
