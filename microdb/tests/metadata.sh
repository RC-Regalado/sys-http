#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT_DIR/bin/microdb"

TMP_DIR=$(mktemp -d /tmp/microdb-metadata.XXXXXX)
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
  if ! printf '%s' "$haystack" | grep -Fq "$needle"; then
    printf 'expected substring not found: %s\n' "$needle" >&2
    printf 'output was:\n%s\n' "$haystack" >&2
    exit 1
  fi
}

write_output=$(run_db 'WRITE note1 notes text/plain hello note
WRITEBLOB blob1 files application/octet-stream payload-blob
EXIT
')
assert_contains "$write_output" 'OK'

if [ ! -d "$TMP_DIR/blobs" ]; then
  printf 'expected blobs directory to exist\n' >&2
  exit 1
fi

read_output=$(run_db 'READ note1
READ blob1
FINDNS notes
FINDTYPE application/octet-stream
EXIT
')
assert_contains "$read_output" 'key=note1 ns=notes type=text/plain'
assert_contains "$read_output" 'data=hello note'
assert_contains "$read_output" 'key=blob1 ns=files type=application/octet-stream'
assert_contains "$read_output" 'data=payload-blob'

count_output=$(run_db 'COUNT notes
COUNT files
COUNT no-such-namespace
EXIT
')
assert_contains "$count_output" 'count=1'
assert_contains "$count_output" 'count=0'

delete_output=$(run_db 'DELETE blob1
READ blob1
EXIT
')
assert_contains "$delete_output" 'key=blob1 ns=files type=application/octet-stream'
assert_contains "$delete_output" 'OK'

blob_count=$(find "$TMP_DIR/blobs" -type f | wc -l)
if [ "$blob_count" -ne 0 ]; then
  printf 'expected blob files to be removed, found %s\n' "$blob_count" >&2
  exit 1
fi

printf 'metadata test passed\n'
