#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT_DIR/bin/microdb"

TMP_DIR=$(mktemp -d /tmp/microdb-compaction.XXXXXX)
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

initial_output=$(run_db 'SET note snapshot-value
SET other second
COMPACT
EXIT
')
assert_contains "$initial_output" 'OK'

if [ ! -f "$TMP_DIR/base.db" ]; then
  printf 'expected base.db to exist after compaction\n' >&2
  exit 1
fi

wal_size=$(wc -c < "$TMP_DIR/data.wal")
if [ "$wal_size" -ne 0 ]; then
  printf 'expected empty wal after compaction, got %s bytes\n' "$wal_size" >&2
  exit 1
fi

restart_output=$(run_db 'GET note
GET other
EXIT
')
assert_contains "$restart_output" 'snapshot-value'
assert_contains "$restart_output" 'second'

overlay_output=$(run_db 'SET note wal-value
DEL other
EXIT
')
assert_contains "$overlay_output" 'OK'

final_output=$(run_db 'GET note
GET other
EXIT
')
assert_contains "$final_output" 'wal-value'
assert_contains "$final_output" '(nil)'

printf 'compaction test passed\n'
