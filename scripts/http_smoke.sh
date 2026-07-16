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

assert_download_size() {
  local name="$1"
  local path="$2"
  local disk_path="$3"
  local rate="${4:-}"
  local max_time="${5:-5}"
  local out status got expected
  local curl_args=()

  if [ -n "${rate}" ]; then
    curl_args=(--limit-rate "${rate}")
  fi

  out="$(mktemp)"
  status="$(curl -sS -o "${out}" -w '%{http_code}' \
    --path-as-is \
    --connect-timeout 1 --max-time "${max_time}" \
    "${curl_args[@]}" \
    "${BASE_URL}${path}")"

  if [ "${status}" != "200" ]; then
    fail "${name}: expected 200, got ${status}"
  fi

  got="$(wc -c <"${out}")"
  expected="$(wc -c <"${disk_path}")"
  if [ "${got}" != "${expected}" ]; then
    fail "${name}: expected ${expected} bytes, got ${got}"
  fi

  printf 'ok - %s\n' "${name}"
}

assert_url_status() {
  local name="$1"
  local expected="$2"
  local url="$3"
  local result status file

  file="$(mktemp)"
  set +e
  status="$(curl -6 -sS -o "${file}" -w '%{http_code}' \
    --connect-timeout 1 --max-time 3 \
    "${url}")"
  local rc=$?
  set -e

  if [ "${rc}" -ne 0 ]; then
    fail "${name}: curl failed with ${rc}"
  fi

  if [ "${status}" != "${expected}" ]; then
    printf 'body: ' >&2
    sed -n '1,20p' "${file}" >&2
    fail "${name}: expected ${expected}, got ${status}"
  fi

  printf 'ok - %s\n' "${name}"
}

assert_raw_status() {
  local name="$1"
  local expected="$2"
  local payload="$3"
  local line

  line="$(printf '%b' "${payload}" | nc -w 2 127.0.0.1 5050 | sed -n '1p')"

  if ! printf '%s' "${line}" | grep -Fq "${expected}"; then
    fail "${name}: expected ${expected}, got ${line}"
  fi

  printf 'ok - %s\n' "${name}"
}

assert_split_header_request() {
  local name="$1"
  local expected="$2"
  local line

  line="$(python3 - <<'PY'
import socket
import time

s = socket.create_connection(("127.0.0.1", 5050), timeout=2)
s.sendall(b"GET /assets/index-B_lim1yr.css HTTP/1.1\r\nHost: local")
time.sleep(0.05)
s.sendall(b"host\r\nConnection: close\r\n\r\n")
print(s.recv(128).split(b"\r\n", 1)[0].decode("ascii", "replace"))
s.close()
PY
)"

  if ! printf '%s' "${line}" | grep -Fq "${expected}"; then
    fail "${name}: expected ${expected}, got ${line}"
  fi

  printf 'ok - %s\n' "${name}"
}

trap cleanup EXIT

make
NOTE_BODY=$'# Smoke note\n\nbody'
printf -v BIG_BODY '%*s' 9000 ''
BIG_BODY="${BIG_BODY// /x}"

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

assert_url_status "GET localhost IPv6" 200 http://localhost:5050/
assert_status "GET /" 200 GET /
assert_status "GET /index.html" 200 GET /index.html
assert_download_size "GET large css complete" /assets/index-B_lim1yr.css templates/assets/index-B_lim1yr.css
assert_download_size "GET large css slow client complete" /assets/index-B_lim1yr.css templates/assets/index-B_lim1yr.css 32k 20
assert_split_header_request "GET split Firefox-like headers" "200"
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

assert_status "GET /index.html with query string" 200 GET "/index.html?x=1"
assert_status "GET /database/smoke with query string" 200 GET "/database/smoke?debug=1&trace"
assert_body_has "GET /database/smoke with query string response" '"found":true'
assert_status "GET /database/notes with query string" 200 GET "/database/notes?limit=1"
assert_body_has "GET /database/notes with query string response" '"namespace":"notes"'
assert_status "GET missing file with query string" 404 GET "/no-existe?x=1"

# Regresion: request con volumen de cabeceras tipico de un navegador real
# (cookie larga + varias cabeceras de fetch metadata) crasheaba el server
# con SIGSEGV -- ver docs/Architecture.md, "Riesgo Critico: SIGSEGV en
# read_incoming". curl con requests chicas nunca disparaba el bug.
printf -v BIG_COOKIE '%*s' 800 ''
BIG_COOKIE="${BIG_COOKIE// /x}"
browser_headers_status="$(curl -sS -o /dev/null -w '%{http_code}' \
  --connect-timeout 1 --max-time 3 \
  -H "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36" \
  -H "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8" \
  -H "Accept-Language: es-ES,es;q=0.9,en;q=0.8" \
  -H "Sec-Fetch-Dest: document" \
  -H "Sec-Fetch-Mode: navigate" \
  -H "Cookie: session=${BIG_COOKIE}" \
  "http://localhost:5050/")"
if [ "${browser_headers_status}" != "200" ]; then
  fail "GET / with browser-sized headers: expected 200, got ${browser_headers_status}"
fi
printf 'ok - %s\n' "GET / with browser-sized headers"

assert_status "PUT unsupported method" 405 PUT /
assert_status "POST without body" 411 POST /database
assert_status "POST /database bad content-type" 415 POST /database '{"key":"bad-ct"}' text/plain
assert_status "POST /database/notes bad content-type" 415 POST /database/notes '# bad' application/json
assert_status "POST payload too large" 413 POST /database "${BIG_BODY}" application/json
assert_raw_status "bad HTTP version" "400" $'GET / HTTP/9.9\r\nHost: localhost\r\n\r\n'

printf 'http smoke: ok\n'
