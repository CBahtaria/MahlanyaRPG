#!/usr/bin/env bash
#
# Verify web/supabase/migrations/001_payment_references.sql against a real,
# throwaway PostgreSQL 16 container.
#
# There is no Supabase CLI project linked in this repo and no local Supabase
# stack, so the migration is applied to stock PostgreSQL with 00_supabase_stubs.sql
# standing in for the parts of a Supabase project it touches (auth schema,
# storage schema, and the anon / authenticated / service_role PostgREST roles
# with their default grants). See that file for where fidelity matters.
#
# Sequence:
#   1. start a disposable postgres:16 container
#   2. apply the stubs
#   3. apply the migration                     <- first apply
#   4. seed a settled row
#   5. apply the migration again               <- proves re-runnable, non-destructive
#   6. run the assertions
#   7. destroy the container
#
# Usage:
#   ./run-tests.sh          # run and tear down
#   KEEP=1 ./run-tests.sh   # leave the container up for manual poking
#
# Exit code 0 means every assertion passed.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MIGRATION="${HERE}/../migrations/001_payment_references.sql"
STUBS="${HERE}/00_supabase_stubs.sql"
ASSERTIONS="${HERE}/01_assert_payment_references.sql"

CONTAINER="${CONTAINER:-mahlanya-pgtest-$$}"
IMAGE="${IMAGE:-docker.io/library/postgres:16}"
KEEP="${KEEP:-0}"

if command -v docker >/dev/null 2>&1; then
  RUNTIME=docker
elif command -v podman >/dev/null 2>&1; then
  RUNTIME=podman
else
  echo "FAIL: neither docker nor podman is on PATH" >&2
  exit 1
fi

for f in "$MIGRATION" "$STUBS" "$ASSERTIONS"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: missing $f" >&2
    exit 1
  fi
done

cleanup() {
  if [ "$KEEP" = "1" ]; then
    echo "KEEP=1 — leaving container ${CONTAINER} running."
  else
    "$RUNTIME" rm -f "$CONTAINER" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

echo "==> starting ${IMAGE} as ${CONTAINER}"
"$RUNTIME" run -d --name "$CONTAINER" \
  -e POSTGRES_PASSWORD=testpw \
  -e POSTGRES_DB=postgres \
  "$IMAGE" >/dev/null

echo "==> waiting for postgres"
for _ in $(seq 1 60); do
  if "$RUNTIME" exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
if ! "$RUNTIME" exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; then
  echo "FAIL: postgres did not become ready in 60s" >&2
  "$RUNTIME" logs "$CONTAINER" >&2 || true
  exit 1
fi

psql_file() {
  "$RUNTIME" exec -i "$CONTAINER" \
    psql -U postgres -d postgres -v ON_ERROR_STOP=1 -q -f - < "$1"
}

psql_cmd() {
  "$RUNTIME" exec -i "$CONTAINER" \
    psql -U postgres -d postgres -v ON_ERROR_STOP=1 -q -c "$1"
}

echo "==> applying Supabase stubs"
psql_file "$STUBS"

echo "==> applying migration (first apply)"
psql_file "$MIGRATION"

echo "==> seeding a settled row before re-applying"
psql_cmd "INSERT INTO payment_references
            (id, service_slug, amount_cents, payer_name, payer_contact, emali_reference, status, confirmed_at)
          VALUES
            ('11111111-1111-1111-1111-111111111111', 'mahlanya-demo-supporter', 10000,
             'Sentinel Row', '+26876000001', 'SENTINEL-REF', 'confirmed', now())
          ON CONFLICT (id) DO NOTHING;"

echo "==> applying migration (second apply — idempotency)"
psql_file "$MIGRATION"

echo "==> running assertions"
psql_file "$ASSERTIONS"

echo
echo "PASS: 001_payment_references.sql verified against PostgreSQL 16"
