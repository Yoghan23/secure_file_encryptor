#!/usr/bin/env bash

# Terminal Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Test Artifacts
TEST_DIR="test_tmp"
BIN="./encryptor"
ORIG_FILE="$TEST_DIR/sample.txt"
ENC_FILE="$TEST_DIR/encrypted.bin"
DEC_FILE="$TEST_DIR/decrypted.txt"
TAMPER_FILE="$TEST_DIR/tampered.bin"
PASS="CorrectPassword123!"
WRONG_PASS="WrongPassword456!"

PASSED_TESTS=0
TOTAL_TESTS=0

# Helper Functions
assert_success() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [ $1 -eq 0 ]; then
        echo -e "[${GREEN}PASS${NC}] $2"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "[${RED}FAIL${NC}] $2"
    fi
}

assert_failure() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [ $1 -ne 0 ]; then
        echo -e "[${GREEN}PASS${NC}] $2"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "[${RED}FAIL${NC}] $2"
    fi
}

# Cleanup on exit
cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

# Environment Setup
mkdir -p "$TEST_DIR"
echo "Confidential data payload for integration testing." > "$ORIG_FILE"

echo "=================================================="
echo " Running Automated Integration Tests for Encryptor"
echo "=================================================="

# Test 1: Compile Check
if [ ! -f "$BIN" ]; then
    echo "Executable $BIN not found. Compiling with make..."
    make > /dev/null 2>&1
fi

# Test 2: Standard Encryption
echo "$PASS" | $BIN -e "$ORIG_FILE" "$ENC_FILE" > /dev/null 2>&1
assert_success $? "1. File Encryption Execution"

# Test 3: Standard Decryption Roundtrip
echo "$PASS" | $BIN -d "$ENC_FILE" "$DEC_FILE" > /dev/null 2>&1
assert_success $? "2. File Decryption Execution"

# Test 4: Payload Content Verification
cmp -s "$ORIG_FILE" "$DEC_FILE"
assert_success $? "3. Decrypted Payload Matches Original Content Exactly"

# Test 5: Rejection of Incorrect Password
rm -f "$DEC_FILE"
echo "$WRONG_PASS" | $BIN -d "$ENC_FILE" "$DEC_FILE" > /dev/null 2>&1
assert_failure $? "4. Authentication Rejection on Wrong Password"

# Test 6: Rejection of Tampered Ciphertext (AES-GCM Integrity Check)
cp "$ENC_FILE" "$TAMPER_FILE"

# Corrupt byte 25 using native dd (flipping bits directly)
printf '\xFF' | dd of="$TAMPER_FILE" bs=1 seek=25 count=1 conv=notrunc > /dev/null 2>&1

echo "$PASS" | $BIN -d "$TAMPER_FILE" "$DEC_FILE" > /dev/null 2>&1
assert_failure $? "5. AEAD Tag Detection on Tampered Ciphertext"

echo "=================================================="
echo -e " Test Results: ${GREEN}$PASSED_TESTS/$TOTAL_TESTS Passed${NC}"
echo "=================================================="

if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    exit 0
else
    exit 1
fi