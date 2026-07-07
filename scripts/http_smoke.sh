#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:5050}"
SERVER_LOG="${SERVER_LOG:-/tmp/sys-http-smoke.log}"
SERVER_PID=""

cleanup() {
  if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  if [ -f "${SERVER_LOG}" ]; then
    printf '%s\n' '--- server log ---' >&2
    sed -n '1,120p' "${SERVER_LOG}" >&2
  fi
  exit 1
}

request() {
  local method="$1"
  local path="$2"
  local body="${3:-}"
  local content_type="${4:-application/json}"
  local out status rc
  out="$(mktemp)"

  set +e
  if [ -n "${body}" ]; then
    status="$(curl -sS -o "${out}" -w '%{http_code}' -X "${method}" \
      --path-as-is \
      --connect-timeout 1 --max-time 3 \
      -H "Content-Type: ${content_type}" \
      --data-binary "${body}" \
      "${BASE_URL}${path}")"
    rc=$?
  else
    status="$(curl -sS -o "${out}" -w '%{http_code}' -X "${method}" \
      --path-as-is \
      --connect-timeout 1 --max-time 3 \
      "${BASE_URL}${path}")"
    rc=$?
  fi
  set -e

  if [ "${rc}" -ne 0 ]; then
    printf 'curl failed for %s %s with code %s\n' "${method}" "${path}" "${rc}" >&2
    printf 'partial body: ' >&2
    sed -n '1,20p' "${out}" >&2
    fail "request timeout or transport error"
  fi

  printf '%s %s' "${status}" "${out}"
}

assert_status() {
  local name="$1"
  local expected="$2"
  local method="$3"
  local path="$4"
  local body="${5:-}"
  local content_type="${6:-application/json}"
  local result status file

  result="$(request "${method}" "${path}" "${body}" "${content_type}")"
  status="${result%% *}"
  file="${result#* }"

  if [ "${status}" != "${expected}" ]; then
    printf 'body: ' >&2
    sed -n '1,20p' "${file}" >&2
    fail "${name}: expected ${expected}, got ${status}"
  fi

  LAST_BODY="${file}"
  printf 'ok - %s\n' "${name}"
}

assert_body_has() {
  local name="$1"
  local needle="$2"
  if ! grep -Fq "${needle}" "${LAST_BODY}"; then
    printf 'body: ' >&2
    sed -n '1,20p' "${LAST_BODY}" >&2
    fail "${name}: missing ${needle}"
  fi
}

trap cleanup EXIT

make
NOTE_BODY=$'# Smoke note\n\nbody'

# Arrancar desde una base de datos totalmente nueva: un data.wal/base.db
# preexistente puede enmascarar bugs que solo aparecen en el primer arranque
# (ver docs/Architecture.md, "Riesgo Critico: Interposicion de Simbolos").
rm -f data.wal base.db
rm -rf blobs

LD_LIBRARY_PATH="./bin:${LD_LIBRARY_PATH:-}" ./bin/server >"${SERVER_LOG}" 2>&1 &
SERVER_PID="$!"

for _ in $(seq 1 30); do
  if curl -sS --connect-timeout 1 --max-time 1 "${BASE_URL}/no-existe" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

kill -0 "${SERVER_PID}" 2>/dev/null || fail "server did not start"

assert_status "GET /" 200 GET /
assert_status "GET /index.html" 200 GET /index.html
assert_status "GET missing file" 404 GET /no-existe
assert_status "GET traversal" 403 GET /../etc/passwd
assert_status "POST /database" 200 POST /database '{"key":"smoke","value":"ok"}'
assert_body_has "POST /database response" '"ok":true'
assert_status "GET /database/smoke" 200 GET /database/smoke
assert_body_has "GET /database/smoke response" '"found":true'
assert_status "POST /database without key" 400 POST /database '{"value":"missing-key"}'
assert_body_has "POST invalid response" '"ok":false'
assert_status "GET /database/namespace/database" 200 GET /database/namespace/database
assert_body_has "GET namespace list response" '"count"'
assert_body_has "GET namespace list has smoke" '"id":"smoke"'
assert_status "GET /database/namespace/does-not-exist" 200 GET /database/namespace/does-not-exist
assert_body_has "GET empty namespace response" '"items":[]'
assert_status "POST /database/notes" 200 POST /database/notes "${NOTE_BODY}" text/markdown
assert_body_has "POST /database/notes response" '"ok":true'
assert_status "GET /database/notes" 200 GET /database/notes
assert_body_has "GET /database/notes response" '"namespace":"notes"'
assert_body_has "GET /database/notes has markdown" '# Smoke note'
assert_status "GET /database/namespace/notes" 200 GET /database/namespace/notes
assert_body_has "GET /database/namespace/notes response" '"namespace":"notes"'

printf 'http smoke: ok\n'
