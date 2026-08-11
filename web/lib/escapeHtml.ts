const HTML_ESCAPE_MAP: Record<string, string> = {
  '&': '&amp;',
  '<': '&lt;',
  '>': '&gt;',
  '"': '&quot;',
  "'": '&#39;',
}

// Escapes user-supplied text before interpolation into Resend `html:` templates.
// Payer-submitted fields (name, contact, eMali reference) are untrusted and must
// never be interpolated raw — an unescaped value renders live in the recipient's inbox.
export function escapeHtml(value: string): string {
  return value.replace(/[&<>"']/g, (char) => HTML_ESCAPE_MAP[char])
}
