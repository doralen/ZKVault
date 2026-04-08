#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <zkvault-binary>" >&2
    exit 1
fi

BIN="$1"
TMPDIR="$(mktemp -d /tmp/zkvault-ctest-errors-XXXXXX)"
trap 'rm -rf "$TMPDIR"' EXIT

assert_contains() {
    local haystack="$1"
    local needle="$2"

    if [[ "$haystack" != *"$needle"* ]]; then
        echo "expected output to contain: $needle" >&2
        echo "actual output:" >&2
        printf '%s\n' "$haystack" >&2
        exit 1
    fi
}

run_ok() {
    local input="$1"
    shift

    local output
    output="$(cd "$TMPDIR" && printf '%s' "$input" | "$BIN" "$@" 2>&1)"
    printf '%s' "$output"
}

run_fail() {
    local input="$1"
    shift

    local output
    set +e
    output="$(cd "$TMPDIR" && printf '%s' "$input" | "$BIN" "$@" 2>&1)"
    local status=$?
    set -e

    if [[ $status -eq 0 ]]; then
        echo "expected command to fail: $*" >&2
        printf '%s\n' "$output" >&2
        exit 1
    fi

    printf '%s' "$output"
}

run_ok $'test-master-password\ntest-master-password\n' init >/dev/null
run_ok $'test-master-password\nentry-password\ninitial note\n' add email >/dev/null

MASTER_BACKUP="$TMPDIR/.zkv_master.backup"
ENTRY_PATH="$TMPDIR/data/email.zkv"
ENTRY_BACKUP="$TMPDIR/data/email.zkv.backup"
cp "$TMPDIR/.zkv_master" "$MASTER_BACKUP"
cp "$ENTRY_PATH" "$ENTRY_BACKUP"

DUPLICATE_INIT_OUTPUT="$(run_fail $'test-master-password\ntest-master-password\n' init)"
assert_contains "$DUPLICATE_INIT_OUTPUT" ".zkv_master already exists"

GET_MISSING_OUTPUT="$(run_fail '' get missing)"
assert_contains "$GET_MISSING_OUTPUT" "entry does not exist"

DELETE_MISSING_OUTPUT="$(run_fail '' delete missing)"
assert_contains "$DELETE_MISSING_OUTPUT" "entry does not exist"

GET_CANCELLED_OUTPUT="$(run_fail '' get email)"
assert_contains "$GET_CANCELLED_OUTPUT" "input cancelled"

printf '{\n' > "$TMPDIR/.zkv_master"
INVALID_MASTER_JSON_OUTPUT="$(run_fail $'test-master-password\n' get email)"
assert_contains "$INVALID_MASTER_JSON_OUTPUT" "invalid .zkv_master JSON"
cp "$MASTER_BACKUP" "$TMPDIR/.zkv_master"

sed 's/"version": 1/"version": 99/' "$MASTER_BACKUP" > "$TMPDIR/.zkv_master"
UNSUPPORTED_MASTER_VERSION_OUTPUT="$(run_fail $'test-master-password\n' get email)"
assert_contains "$UNSUPPORTED_MASTER_VERSION_OUTPUT" "unsupported .zkv_master version"
cp "$MASTER_BACKUP" "$TMPDIR/.zkv_master"

sed 's/"kdf": "scrypt"/"kdf": "pbkdf2"/' "$MASTER_BACKUP" > "$TMPDIR/.zkv_master"
UNSUPPORTED_MASTER_KDF_OUTPUT="$(run_fail $'test-master-password\n' get email)"
assert_contains "$UNSUPPORTED_MASTER_KDF_OUTPUT" "unsupported .zkv_master kdf"
cp "$MASTER_BACKUP" "$TMPDIR/.zkv_master"

printf '{\n' > "$ENTRY_PATH"
INVALID_ENTRY_JSON_OUTPUT="$(run_fail $'test-master-password\n' get email)"
assert_contains "$INVALID_ENTRY_JSON_OUTPUT" "invalid encrypted entry JSON"
cp "$ENTRY_BACKUP" "$ENTRY_PATH"

sed 's/"version": 1/"version": 99/' "$ENTRY_BACKUP" > "$ENTRY_PATH"
UNSUPPORTED_ENTRY_VERSION_OUTPUT="$(run_fail $'test-master-password\n' get email)"
assert_contains "$UNSUPPORTED_ENTRY_VERSION_OUTPUT" "unsupported encrypted entry version"
