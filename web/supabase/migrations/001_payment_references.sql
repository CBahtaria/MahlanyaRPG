-- eMali manual-payment reconciliation for the MahlanyaRPG web/ companion app.
-- Re-runnable: every statement is idempotent (IF NOT EXISTS / ON CONFLICT DO NOTHING).

CREATE TABLE IF NOT EXISTS payment_references (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  service_slug text NOT NULL,
  amount_cents integer NOT NULL CHECK (amount_cents > 0),
  currency text NOT NULL DEFAULT 'SZL',
  payer_name text NOT NULL,
  payer_contact text NOT NULL,
  emali_reference text NOT NULL,
  status text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'confirmed', 'rejected')),
  created_at timestamptz NOT NULL DEFAULT now(),
  confirmed_at timestamptz,
  artifact_delivered_at timestamptz
);

ALTER TABLE payment_references ENABLE ROW LEVEL SECURITY;
-- No policies are created deliberately: web/ never issues a browser-side Supabase
-- key (no NEXT_PUBLIC anon key exists in this app), so every read/write goes
-- through the server-only service-role key, which bypasses RLS by design. RLS is
-- enabled with zero policies as a fail-closed backstop — if an anon key is ever
-- accidentally exposed in a future change, it grants access to nothing.

INSERT INTO storage.buckets (id, name, public)
VALUES ('mahlanya-builds', 'mahlanya-builds', false)
ON CONFLICT (id) DO NOTHING;
-- Private bucket, no storage.objects policies added — same fail-closed reasoning
-- as above. Upload (by the operator, out-of-band) and signed-URL creation (on
-- confirm) both happen through the service-role key server-side.
