#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT_DIR/bin/microdb"

TMP_DIR=$(mktemp -d /tmp/microdb-recovery.XXXXXX)
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

run_db() {
  input=$1
  (
    cd "$TMP_DIR"
    printf '%s' "$input" | "$BIN" "$TMP_DIR/data.wal" --sync
  )
}

assert_contains() {
  haystack=$1
  needle=$2
  if ! printf '%s' "$haystack" | grep -Fqx "$needle"; then
    printf 'expected line not found: %s\n' "$needle" >&2
    printf 'output was:\n%s\n' "$haystack" >&2
    exit 1
  fi
}

initial_output=$(run_db 'SET foo bar
SET note hello world
DEL foo
EXIT
')
assert_contains "$initial_output" 'OK'

restart_output=$(run_db 'GET foo
GET note
EXIT
')
assert_contains "$restart_output" '(nil)'
assert_contains "$restart_output" 'hello world'

printf '\xFE\xED\xFA' >> "$TMP_DIR/data.wal"

truncated_tail_output=$(run_db 'GET note
EXIT
')
assert_contains "$truncated_tail_output" 'hello world'

printf 'recovery test passed\n'
