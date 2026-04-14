#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ntag424_changekey_utils.h"

namespace {

int fail(const char *message) {
  std::fprintf(stderr, "FAIL: %s\n", message);
  return 1;
}

bool expect_bytes(const uint8_t *actual, const uint8_t *expected, size_t len,
                  const char *label) {
  if (std::memcmp(actual, expected, len) == 0) {
    return true;
  }

  std::fprintf(stderr, "FAIL: %s mismatch\n", label);
  std::fprintf(stderr, "  expected: ");
  for (size_t i = 0; i < len; ++i) {
    std::fprintf(stderr, "%02X", expected[i]);
  }
  std::fprintf(stderr, "\n  actual:   ");
  for (size_t i = 0; i < len; ++i) {
    std::fprintf(stderr, "%02X", actual[i]);
  }
  std::fprintf(stderr, "\n");
  return false;
}

}

int main() {
  const uint8_t oldkey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                              0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const uint8_t newkey[16] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
                              0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
  const uint32_t jamcrc = 0x78563412;
  uint8_t keydata[32] = {0};

  const uint8_t non_master_len = ntag424_build_changekey_payload(
      oldkey, newkey, 4, 0x01, jamcrc, keydata);
  if (non_master_len != 21) {
    return fail("non-master payload length should be 21 bytes");
  }

  const uint8_t expected_non_master[21] = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x12, 0x34, 0x56, 0x78};
  if (!expect_bytes(keydata, expected_non_master, sizeof(expected_non_master),
                    "non-master ChangeKey payload")) {
    return 1;
  }

  std::memset(keydata, 0, sizeof(keydata));
  const uint8_t master_len =
      ntag424_build_changekey_payload(oldkey, newkey, 0, 0x00, jamcrc, keydata);
  if (master_len != 17) {
    return fail("master payload length should be 17 bytes");
  }

  const uint8_t expected_master[17] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA,
                                       0x99, 0x88, 0x77, 0x66, 0x55, 0x44,
                                       0x33, 0x22, 0x11, 0x00, 0x00};
  if (!expect_bytes(keydata, expected_master, sizeof(expected_master),
                    "master ChangeKey payload")) {
    return 1;
  }

  const uint8_t full_mode_success[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x91, 0x00};
  if (!ntag424_changekey_succeeded(full_mode_success,
                                   sizeof(full_mode_success))) {
    return fail("FULL-mode status bytes should be read from the response tail");
  }

  const uint8_t legacy_false_negative[2] = {0x91, 0x00};
  if (!ntag424_changekey_succeeded(legacy_false_negative,
                                   sizeof(legacy_false_negative))) {
    return fail("plain status-only success response should still pass");
  }

  const uint8_t auth_error[4] = {0x00, 0x00, 0x91, 0xAE};
  if (ntag424_changekey_succeeded(auth_error, sizeof(auth_error))) {
    return fail("non-success status should fail");
  }

  const uint8_t too_short[1] = {0x91};
  if (ntag424_changekey_succeeded(too_short, sizeof(too_short))) {
    return fail("short response should fail");
  }

  std::puts("PASS: ChangeKey helper behavior matches expected protocol");
  return 0;
}
