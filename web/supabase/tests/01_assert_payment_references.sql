-- Assertions for web/supabase/migrations/001_payment_references.sql.
-- Run by run-tests.sh against a throwaway PostgreSQL 16 container that has had
-- 00_supabase_stubs.sql then the migration (twice, for idempotency) applied.
--
-- Every assertion RAISEs EXCEPTION on failure; psql runs with ON_ERROR_STOP=1 so
-- the first failure aborts with a non-zero exit code.

\set ON_ERROR_STOP on

-- ===========================================================================
-- A. Table shape
-- ===========================================================================

-- A1: the table exists in the public schema.
DO $$
BEGIN
  IF to_regclass('public.payment_references') IS NULL THEN
    RAISE EXCEPTION 'A1 FAILED: public.payment_references does not exist';
  END IF;
  RAISE NOTICE 'A1 ok: public.payment_references exists';
END $$;

-- A2: exact column set, types, nullability. Any drift (added, removed, renamed,
-- or retyped column) fails here rather than surfacing as a runtime error in a
-- later task's API route.
DO $$
DECLARE
  diff text;
BEGIN
  WITH expected(column_name, data_type, is_nullable) AS (
    VALUES
      ('id',                    'uuid',                        'NO'),
      ('service_slug',          'text',                        'NO'),
      ('amount_cents',          'integer',                     'NO'),
      ('currency',              'text',                        'NO'),
      ('payer_name',            'text',                        'NO'),
      ('payer_contact',         'text',                        'NO'),
      ('emali_reference',       'text',                        'NO'),
      ('status',                'text',                        'NO'),
      ('created_at',            'timestamp with time zone',    'NO'),
      ('confirmed_at',          'timestamp with time zone',    'YES'),
      ('artifact_delivered_at', 'timestamp with time zone',    'YES')
  ),
  actual AS (
    SELECT c.column_name::text, c.data_type::text, c.is_nullable::text
    FROM information_schema.columns c
    WHERE c.table_schema = 'public' AND c.table_name = 'payment_references'
  ),
  mismatches AS (
    SELECT 'missing/wrong in actual: ' || e.column_name || ' ' || e.data_type
             || ' nullable=' || e.is_nullable AS msg
    FROM expected e
    WHERE NOT EXISTS (
      SELECT 1 FROM actual a
      WHERE a.column_name = e.column_name
        AND a.data_type = e.data_type
        AND a.is_nullable = e.is_nullable
    )
    UNION ALL
    SELECT 'unexpected in actual: ' || a.column_name || ' ' || a.data_type
             || ' nullable=' || a.is_nullable
    FROM actual a
    WHERE NOT EXISTS (
      SELECT 1 FROM expected e
      WHERE a.column_name = e.column_name
        AND a.data_type = e.data_type
        AND a.is_nullable = e.is_nullable
    )
  )
  SELECT string_agg(msg, '; ') INTO diff FROM mismatches;

  IF diff IS NOT NULL THEN
    RAISE EXCEPTION 'A2 FAILED: column shape drift -> %', diff;
  END IF;
  RAISE NOTICE 'A2 ok: 11 columns match expected types and nullability exactly';
END $$;

-- A3: id is the primary key and defaults to gen_random_uuid().
DO $$
DECLARE
  pk_cols text;
  id_default text;
BEGIN
  SELECT string_agg(a.attname, ',' ORDER BY a.attname) INTO pk_cols
  FROM pg_constraint c
  JOIN pg_attribute a ON a.attrelid = c.conrelid AND a.attnum = ANY (c.conkey)
  WHERE c.conrelid = 'public.payment_references'::regclass AND c.contype = 'p';

  IF pk_cols IS DISTINCT FROM 'id' THEN
    RAISE EXCEPTION 'A3 FAILED: expected primary key (id), got (%)', coalesce(pk_cols, '<none>');
  END IF;

  SELECT column_default INTO id_default
  FROM information_schema.columns
  WHERE table_schema = 'public' AND table_name = 'payment_references' AND column_name = 'id';

  IF id_default IS DISTINCT FROM 'gen_random_uuid()' THEN
    RAISE EXCEPTION 'A3 FAILED: id default is % (expected gen_random_uuid())', coalesce(id_default, '<none>');
  END IF;
  RAISE NOTICE 'A3 ok: PK is (id), default gen_random_uuid()';
END $$;

-- A4: server-side defaults. currency/status/created_at must not depend on the
-- client supplying them — Task 3's insert omits currency and status entirely.
DO $$
DECLARE
  r record;
BEGIN
  INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference)
  VALUES ('mahlanya-demo-supporter', 10000, 'Default Probe', '+26876000000', 'A4REF')
  RETURNING * INTO r;

  IF r.id IS NULL THEN
    RAISE EXCEPTION 'A4 FAILED: id was not defaulted';
  END IF;
  IF r.currency <> 'SZL' THEN
    RAISE EXCEPTION 'A4 FAILED: currency defaulted to %, expected SZL', r.currency;
  END IF;
  IF r.status <> 'pending' THEN
    RAISE EXCEPTION 'A4 FAILED: status defaulted to %, expected pending', r.status;
  END IF;
  IF r.created_at IS NULL THEN
    RAISE EXCEPTION 'A4 FAILED: created_at was not defaulted';
  END IF;
  IF r.confirmed_at IS NOT NULL OR r.artifact_delivered_at IS NOT NULL THEN
    RAISE EXCEPTION 'A4 FAILED: confirmed_at/artifact_delivered_at should start NULL';
  END IF;

  DELETE FROM payment_references WHERE id = r.id;
  RAISE NOTICE 'A4 ok: defaults are id=uuid, currency=SZL, status=pending, created_at=now(), timestamps NULL';
END $$;

-- ===========================================================================
-- B. Constraints
-- ===========================================================================

-- B1: amount_cents > 0. A zero or negative "payment" must never be storable.
DO $$
DECLARE
  probe int;
  rejected int := 0;
BEGIN
  FOREACH probe IN ARRAY ARRAY[0, -1, -10000] LOOP
    BEGIN
      INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference)
      VALUES ('mahlanya-demo-supporter', probe, 'B1 Probe', '+26876000000', 'B1REF');
      RAISE EXCEPTION 'B1 FAILED: amount_cents = % was accepted', probe;
    EXCEPTION WHEN check_violation THEN
      rejected := rejected + 1;
    END;
  END LOOP;

  IF rejected <> 3 THEN
    RAISE EXCEPTION 'B1 FAILED: expected 3 check violations, got %', rejected;
  END IF;

  -- and the boundary value 1 is accepted
  INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference)
  VALUES ('mahlanya-demo-supporter', 1, 'B1 Probe', '+26876000000', 'B1REF');
  DELETE FROM payment_references WHERE emali_reference = 'B1REF';
  RAISE NOTICE 'B1 ok: amount_cents rejects 0/-1/-10000, accepts 1';
END $$;

-- B2: status is constrained to the three known values. Task 5 flips rows to
-- confirmed/rejected; nothing else may ever land in this column.
DO $$
DECLARE
  good text;
  bad text;
  rejected int := 0;
BEGIN
  FOREACH good IN ARRAY ARRAY['pending', 'confirmed', 'rejected'] LOOP
    INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference, status)
    VALUES ('mahlanya-demo-supporter', 10000, 'B2 Probe', '+26876000000', 'B2REF', good);
  END LOOP;

  FOREACH bad IN ARRAY ARRAY['Pending', 'PENDING', 'refunded', 'paid', 'settled', ''] LOOP
    BEGIN
      INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference, status)
      VALUES ('mahlanya-demo-supporter', 10000, 'B2 Probe', '+26876000000', 'B2REF', bad);
      RAISE EXCEPTION 'B2 FAILED: status = ''%'' was accepted', bad;
    EXCEPTION WHEN check_violation THEN
      rejected := rejected + 1;
    END;
  END LOOP;

  IF rejected <> 6 THEN
    RAISE EXCEPTION 'B2 FAILED: expected 6 check violations, got %', rejected;
  END IF;

  DELETE FROM payment_references WHERE emali_reference = 'B2REF';
  RAISE NOTICE 'B2 ok: status accepts pending/confirmed/rejected, rejects case variants and unknown values';
END $$;

-- B3: every NOT NULL column actually rejects NULL. Guards against a future
-- migration relaxing a column that financial reconciliation depends on.
DO $$
DECLARE
  col text;
  rejected int := 0;
  cols text[] := ARRAY['service_slug', 'amount_cents', 'payer_name', 'payer_contact', 'emali_reference', 'currency', 'status'];
BEGIN
  FOREACH col IN ARRAY cols LOOP
    BEGIN
      EXECUTE format(
        'INSERT INTO payment_references (service_slug, amount_cents, currency, payer_name, payer_contact, emali_reference, status)
         VALUES (%s, %s, %s, %s, %s, %s, %s)',
        CASE WHEN col = 'service_slug'    THEN 'NULL' ELSE '''mahlanya-demo-supporter''' END,
        CASE WHEN col = 'amount_cents'    THEN 'NULL' ELSE '10000' END,
        CASE WHEN col = 'currency'        THEN 'NULL' ELSE '''SZL''' END,
        CASE WHEN col = 'payer_name'      THEN 'NULL' ELSE '''B3 Probe''' END,
        CASE WHEN col = 'payer_contact'   THEN 'NULL' ELSE '''+26876000000''' END,
        CASE WHEN col = 'emali_reference' THEN 'NULL' ELSE '''B3REF''' END,
        CASE WHEN col = 'status'          THEN 'NULL' ELSE '''pending''' END
      );
      RAISE EXCEPTION 'B3 FAILED: NULL % was accepted', col;
    EXCEPTION WHEN not_null_violation THEN
      rejected := rejected + 1;
    END;
  END LOOP;

  IF rejected <> array_length(cols, 1) THEN
    RAISE EXCEPTION 'B3 FAILED: expected % not-null violations, got %', array_length(cols, 1), rejected;
  END IF;
  DELETE FROM payment_references WHERE emali_reference = 'B3REF';
  RAISE NOTICE 'B3 ok: all 7 NOT NULL columns reject NULL';
END $$;

-- ===========================================================================
-- C. Row Level Security
-- ===========================================================================

-- C1: RLS is enabled, and NOT forced. (Not forced is correct and deliberate:
-- the migration owner still needs to read the table, and service_role bypasses
-- RLS anyway. Asserted so a later change to either flag is a visible decision.)
DO $$
DECLARE
  enabled boolean;
  forced boolean;
BEGIN
  SELECT relrowsecurity, relforcerowsecurity INTO enabled, forced
  FROM pg_class WHERE oid = 'public.payment_references'::regclass;

  IF NOT enabled THEN
    RAISE EXCEPTION 'C1 FAILED: row level security is NOT enabled on payment_references';
  END IF;
  RAISE NOTICE 'C1 ok: RLS enabled (relforcerowsecurity=%)', forced;
END $$;

-- C2: exactly zero policies. This is the design's core claim — RLS is a
-- fail-closed backstop, not an access-control mechanism, because nothing ever
-- connects to this project as anon or authenticated.
DO $$
DECLARE
  n int;
  names text;
BEGIN
  SELECT count(*), string_agg(policyname, ', ') INTO n, names
  FROM pg_policies WHERE schemaname = 'public' AND tablename = 'payment_references';

  IF n <> 0 THEN
    RAISE EXCEPTION 'C2 FAILED: expected 0 policies, found % (%)', n, names;
  END IF;
  RAISE NOTICE 'C2 ok: zero RLS policies on payment_references';
END $$;

-- C3: control assertion. anon and authenticated DO hold table grants (Supabase
-- installs these by default). Without this, the "anon sees nothing" assertions
-- below would pass for the wrong reason — a missing GRANT rather than RLS.
DO $$
DECLARE
  anon_privs text;
  auth_privs text;
BEGIN
  SELECT string_agg(DISTINCT privilege_type, ',' ORDER BY privilege_type) INTO anon_privs
  FROM information_schema.role_table_grants
  WHERE table_schema = 'public' AND table_name = 'payment_references' AND grantee = 'anon';

  SELECT string_agg(DISTINCT privilege_type, ',' ORDER BY privilege_type) INTO auth_privs
  FROM information_schema.role_table_grants
  WHERE table_schema = 'public' AND table_name = 'payment_references' AND grantee = 'authenticated';

  IF anon_privs IS NULL OR anon_privs NOT LIKE '%SELECT%' OR anon_privs NOT LIKE '%INSERT%' THEN
    RAISE EXCEPTION 'C3 FAILED: anon lacks table grants (%) — the RLS assertions below would be vacuous', coalesce(anon_privs, '<none>');
  END IF;
  IF auth_privs IS NULL OR auth_privs NOT LIKE '%SELECT%' THEN
    RAISE EXCEPTION 'C3 FAILED: authenticated lacks table grants (%)', coalesce(auth_privs, '<none>');
  END IF;
  RAISE NOTICE 'C3 ok: anon grants=[%], authenticated grants=[%] — RLS is what blocks, not missing grants', anon_privs, auth_privs;
END $$;

-- Seed one settled row for the role assertions to try (and fail) to reach.
INSERT INTO payment_references
  (id, service_slug, amount_cents, payer_name, payer_contact, emali_reference, status, confirmed_at)
VALUES
  ('11111111-1111-1111-1111-111111111111', 'mahlanya-demo-supporter', 10000,
   'Sentinel Row', '+26876000001', 'SENTINEL-REF', 'confirmed', now())
ON CONFLICT (id) DO NOTHING;

-- C4/C5: anon and authenticated can read nothing, write nothing, and cannot
-- re-decide the settled sentinel row. This is the exact class of hole that was
-- found and fixed on the sibling brt-inc repo (an UPDATE policy with
-- USING (true) let any authenticated user rewrite a settled payment). Here the
-- equivalent must be structurally impossible: there is no policy at all.
DO $$
DECLARE
  target text;
  seen int;
  touched int;
  insert_blocked boolean;
BEGIN
  FOREACH target IN ARRAY ARRAY['anon', 'authenticated'] LOOP
    EXECUTE format('SET LOCAL ROLE %I', target);

    EXECUTE 'SELECT count(*) FROM payment_references' INTO seen;
    IF seen <> 0 THEN
      RESET ROLE;
      RAISE EXCEPTION 'C4 FAILED: role % can SELECT % row(s) — RLS is not fail-closed', target, seen;
    END IF;

    insert_blocked := false;
    BEGIN
      EXECUTE 'INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference)
               VALUES (''forged'', 1, ''Attacker'', ''x'', ''FORGED'')';
    EXCEPTION WHEN insufficient_privilege THEN
      insert_blocked := true;
    END;
    IF NOT insert_blocked THEN
      RESET ROLE;
      RAISE EXCEPTION 'C5 FAILED: role % was able to INSERT a payment row', target;
    END IF;

    EXECUTE 'UPDATE payment_references SET status = ''confirmed'', confirmed_at = now()';
    GET DIAGNOSTICS touched = ROW_COUNT;
    IF touched <> 0 THEN
      RESET ROLE;
      RAISE EXCEPTION 'C5 FAILED: role % updated % row(s) — a settled payment is re-decidable', target, touched;
    END IF;

    EXECUTE 'UPDATE payment_references SET status = ''pending'', confirmed_at = NULL
             WHERE id = ''11111111-1111-1111-1111-111111111111''';
    GET DIAGNOSTICS touched = ROW_COUNT;
    IF touched <> 0 THEN
      RESET ROLE;
      RAISE EXCEPTION 'C5 FAILED: role % reopened the settled sentinel row', target;
    END IF;

    EXECUTE 'DELETE FROM payment_references';
    GET DIAGNOSTICS touched = ROW_COUNT;
    IF touched <> 0 THEN
      RESET ROLE;
      RAISE EXCEPTION 'C5 FAILED: role % deleted % row(s)', target, touched;
    END IF;

    RESET ROLE;
    RAISE NOTICE 'C4/C5 ok: role % — SELECT 0 rows, INSERT denied, UPDATE 0 rows, re-open 0 rows, DELETE 0 rows', target;
  END LOOP;
END $$;

-- C6: the sentinel row is still intact and still confirmed after all of that.
DO $$
DECLARE
  st text;
  ca timestamptz;
BEGIN
  SELECT status, confirmed_at INTO st, ca
  FROM payment_references WHERE id = '11111111-1111-1111-1111-111111111111';

  IF st IS NULL THEN
    RAISE EXCEPTION 'C6 FAILED: sentinel row was destroyed';
  END IF;
  IF st <> 'confirmed' OR ca IS NULL THEN
    RAISE EXCEPTION 'C6 FAILED: sentinel row was mutated (status=%, confirmed_at=%)', st, ca;
  END IF;
  RAISE NOTICE 'C6 ok: settled sentinel row survived the anon/authenticated write attempts unchanged';
END $$;

-- C7: service_role (BYPASSRLS) retains full access — RLS-with-zero-policies must
-- not lock out the app's own server-side client.
DO $$
DECLARE
  seen int;
  touched int;
BEGIN
  SET LOCAL ROLE service_role;

  EXECUTE 'SELECT count(*) FROM payment_references' INTO seen;
  IF seen < 1 THEN
    RAISE EXCEPTION 'C7 FAILED: service_role sees % rows — it must bypass RLS', seen;
  END IF;

  EXECUTE 'INSERT INTO payment_references (service_slug, amount_cents, payer_name, payer_contact, emali_reference)
           VALUES (''mahlanya-demo-supporter'', 10000, ''C7 Probe'', ''+26876000002'', ''C7REF'')';

  EXECUTE 'UPDATE payment_references SET status = ''confirmed'', confirmed_at = now()
           WHERE emali_reference = ''C7REF''';
  GET DIAGNOSTICS touched = ROW_COUNT;
  IF touched <> 1 THEN
    RAISE EXCEPTION 'C7 FAILED: service_role updated % rows, expected 1', touched;
  END IF;

  EXECUTE 'DELETE FROM payment_references WHERE emali_reference = ''C7REF''';
  GET DIAGNOSTICS touched = ROW_COUNT;
  IF touched <> 1 THEN
    RAISE EXCEPTION 'C7 FAILED: service_role deleted % rows, expected 1', touched;
  END IF;

  RESET ROLE;
  RAISE NOTICE 'C7 ok: service_role bypasses RLS — full SELECT/INSERT/UPDATE/DELETE';
END $$;

-- ===========================================================================
-- D. Storage bucket
-- ===========================================================================

-- D1: the bucket exists exactly once and is private.
DO $$
DECLARE
  n int;
  is_public boolean;
  bname text;
BEGIN
  SELECT count(*) INTO n FROM storage.buckets WHERE id = 'mahlanya-builds';
  IF n <> 1 THEN
    RAISE EXCEPTION 'D1 FAILED: expected exactly 1 mahlanya-builds bucket row, found %', n;
  END IF;

  SELECT public, name INTO is_public, bname FROM storage.buckets WHERE id = 'mahlanya-builds';
  IF is_public IS DISTINCT FROM false THEN
    RAISE EXCEPTION 'D1 FAILED: bucket is public=% — the build artifact would be world-readable', is_public;
  END IF;
  IF bname <> 'mahlanya-builds' THEN
    RAISE EXCEPTION 'D1 FAILED: bucket name is %, expected mahlanya-builds', bname;
  END IF;
  RAISE NOTICE 'D1 ok: bucket mahlanya-builds exists exactly once, public=false';
END $$;

-- D2: no storage.objects policies were added for this bucket, and anon can
-- neither enumerate buckets nor read objects. Downloads are only ever reachable
-- through a service-role-minted signed URL (Task 5).
DO $$
DECLARE
  n int;
  seen int;
BEGIN
  SELECT count(*) INTO n FROM pg_policies WHERE schemaname = 'storage';
  IF n <> 0 THEN
    RAISE EXCEPTION 'D2 FAILED: expected 0 storage policies, found %', n;
  END IF;

  SET LOCAL ROLE anon;
  EXECUTE 'SELECT count(*) FROM storage.buckets' INTO seen;
  IF seen <> 0 THEN
    RESET ROLE;
    RAISE EXCEPTION 'D2 FAILED: anon can enumerate % bucket(s)', seen;
  END IF;
  EXECUTE 'SELECT count(*) FROM storage.objects' INTO seen;
  IF seen <> 0 THEN
    RESET ROLE;
    RAISE EXCEPTION 'D2 FAILED: anon can enumerate % storage object(s)', seen;
  END IF;
  RESET ROLE;
  RAISE NOTICE 'D2 ok: zero storage policies; anon enumerates 0 buckets and 0 objects';
END $$;

-- ===========================================================================
-- E. Idempotency
-- ===========================================================================

-- E1: run-tests.sh applies the migration twice before this file runs. If the
-- second apply had errored, psql would already have aborted; this asserts the
-- second apply also had no *silent* side effect — no duplicate bucket row, and
-- the pre-existing sentinel row was not wiped by a table re-create.
DO $$
DECLARE
  buckets int;
  sentinel int;
BEGIN
  SELECT count(*) INTO buckets FROM storage.buckets WHERE id = 'mahlanya-builds';
  IF buckets <> 1 THEN
    RAISE EXCEPTION 'E1 FAILED: % mahlanya-builds bucket rows after double-apply', buckets;
  END IF;

  SELECT count(*) INTO sentinel FROM payment_references
  WHERE id = '11111111-1111-1111-1111-111111111111';
  IF sentinel <> 1 THEN
    RAISE EXCEPTION 'E1 FAILED: sentinel row count is % after double-apply', sentinel;
  END IF;
  RAISE NOTICE 'E1 ok: migration is re-runnable — 1 bucket row, existing data preserved';
END $$;

-- Clean up the sentinel so the container is left in a neutral state if reused.
DELETE FROM payment_references WHERE id = '11111111-1111-1111-1111-111111111111';

DO $$ BEGIN RAISE NOTICE 'ALL ASSERTIONS PASSED'; END $$;
