#!/bin/bash
set -u

cd "$(dirname "$0")/.." || exit 2

echo "=== PrintHex duplicate proof ==="
echo "Definitions found:"
grep -n "^[[:space:]]*void[[:space:]]\+PrintHex[[:space:]]*(" aescmac.cpp Adafruit_PN532_NTAG424.cpp || true
echo
echo "Symbol names found:"
grep -n "PrintHex" aescmac.cpp aescmac.h Adafruit_PN532_NTAG424.cpp || true
echo

cpp_defs=$(grep -c "^[[:space:]]*void[[:space:]]\+PrintHex[[:space:]]*(" aescmac.cpp)
main_defs=$(grep -c "^[[:space:]]*void[[:space:]]\+PrintHex[[:space:]]*(" Adafruit_PN532_NTAG424.cpp)
header_decls=$(grep -c "^[[:space:]]*void[[:space:]]\+PrintHex[[:space:]]*(" aescmac.h)

if [ "$cpp_defs" -gt 0 ] && [ "$main_defs" -gt 0 ]; then
  echo "FAIL: duplicate PrintHex definitions detected"
  exit 1
fi

if [ "$header_decls" -gt 0 ]; then
  echo "FAIL: duplicate PrintHex declaration still present in aescmac.h"
  exit 1
fi

if [ "$main_defs" -eq 1 ]; then
  echo "PASS: single canonical PrintHex remains"
  exit 0
fi

echo "FAIL: expected canonical PrintHex definition not found"
exit 1
