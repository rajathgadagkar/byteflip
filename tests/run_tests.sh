#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REVERSER="$ROOT_DIR/build/bin/byteflip"
VERIFIER="$ROOT_DIR/build/bin/byteflip-verify"
DATAGEN="$ROOT_DIR/build/bin/byteflip-datagen"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$TMP_DIR"

pass() { printf "  [PASS] %s\n" "$1"; }
fail() { printf "  [FAIL] %s\n" "$1" >&2; exit 1; }

assert_eq() {
    local actual="$1"
    local expected="$2"
    local label="$3"
    if [[ "$actual" == "$expected" ]]; then
        pass "$label"
    else
        fail "$label (expected '$expected', got '$actual')"
    fi
}

echo "Running ByteFlip automated test suite..."

# 1. Mode 0: Block-wise reversal
printf "ABCDEFGH" > block.txt
"$REVERSER" block.txt 0 4 >/dev/null
assert_eq "$(cat output/0_block.txt)" "DCBAHGFE" "Mode 0: exact block reversal"

REPORT="$("$VERIFIER" output/0_block.txt block.txt output 0 4)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" || fail "Mode 0 verifier content check"
echo "$REPORT" | grep -q "Both Files Sizes are Same: Yes" || fail "Mode 0 verifier size check"
assert_eq "$(echo "$REPORT" | wc -l | tr -d ' ')" "30" "Mode 0 verifier outputs 30 lines"

# 2. Mode 1: Full-file reversal
printf "Hello, World!" > full.txt
"$REVERSER" full.txt 1 >/dev/null
assert_eq "$(cat output/1_full.txt)" "!dlroW ,olleH" "Mode 1: full-file reversal"

REPORT="$("$VERIFIER" output/1_full.txt full.txt output 1)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" || fail "Mode 1 verifier content check"
pass "Mode 1: verifier confirmation"

# 3. Mode 2: Partial-range reversal
printf "Hello, World!" > partial.txt
"$REVERSER" partial.txt 2 2 7 >/dev/null
assert_eq "$(cat output/2_partial.txt)" "eHllo, W!dlro" "Mode 2: partial-range reversal"

REPORT="$("$VERIFIER" output/2_partial.txt partial.txt output 2 2 7)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" || fail "Mode 2 verifier content check"
pass "Mode 2: verifier confirmation"

# 4. Empty file handling
: > empty.txt
"$REVERSER" empty.txt 1 >/dev/null
assert_eq "$(wc -c < output/1_empty.txt | tr -d ' ')" "0" "Empty file: zero-byte reversal"
REPORT="$("$VERIFIER" output/1_empty.txt empty.txt output 1)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" || fail "Empty file verifier check"
pass "Empty file: verifier confirmation"

# 5. Multi-MiB streaming test (> 1 MiB buffer boundary)
"$DATAGEN" large.bin 2500000 >/dev/null
"$REVERSER" large.bin 0 131072 >/dev/null
REPORT="$("$VERIFIER" output/0_large.bin large.bin output 0 131072)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" \
    || fail "Multi-MiB Mode 0 verification"
pass "Multi-MiB streaming: Mode 0 (2.5 MB, 128 KiB blocks)"

"$REVERSER" large.bin 1 >/dev/null
REPORT="$("$VERIFIER" output/1_large.bin large.bin output 1)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" \
    || fail "Multi-MiB Mode 1 verification"
pass "Multi-MiB streaming: Mode 1 (2.5 MB full reversal)"

"$REVERSER" large.bin 2 500000 1800000 >/dev/null
REPORT="$("$VERIFIER" output/2_large.bin large.bin output 2 500000 1800000)"
echo "$REPORT" | grep -q "Whether file contents are correctly processed: Yes" \
    || fail "Multi-MiB Mode 2 verification"
pass "Multi-MiB streaming: Mode 2 (2.5 MB partial range)"

# 6. Strict POSIX permissions check (dir: 0700, output file: 0600)
get_octal_perm() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        stat -f '%Lp' "$1"
    else
        stat -c '%a' "$1"
    fi
}
DIR_PERM="$(get_octal_perm output)"
FILE_PERM="$(get_octal_perm output/1_full.txt)"
assert_eq "$DIR_PERM" "700" "POSIX permissions: output directory is 0700"
assert_eq "$FILE_PERM" "600" "POSIX permissions: output file is 0600"

# 7. Error handling checks
if "$REVERSER" block.txt 9 >/dev/null 2>&1; then
    fail "Error handling: invalid flag should fail"
fi
pass "Error handling: rejects invalid flag"

if "$REVERSER" partial.txt 2 8 2 >/dev/null 2>&1; then
    fail "Error handling: start > end should fail"
fi
pass "Error handling: rejects inverted range"

echo "All tests passed!"
