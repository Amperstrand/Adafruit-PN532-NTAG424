#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ntag424_crypto.h"

uint8_t ntag424_authresponse_TI[NTAG424_AUTHRESPONSE_TI_SIZE] = {0x00, 0x00, 0x00, 0x00};

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

} // namespace

int main() {
  int passed = 0;

  // --- rotl test ---
  {
    uint8_t input[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t output[8] = {0};
    uint8_t ret = ntag424_rotl(input, output, 8, 3);
    if (ret != 1) {
      return fail("rotl should return 1 on success");
    }
    uint8_t expected[8] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x01, 0x02, 0x03};
    if (!expect_bytes(output, expected, 8, "rotl(8, 3)")) {
      return 1;
    }
    std::puts("PASS: rotl");
    passed++;
  }

  // --- rotl edge case: rotation == bufferlen ---
  {
    uint8_t input[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t output[4] = {0};
    uint8_t ret = ntag424_rotl(input, output, 4, 4);
    if (ret != 1) {
      return fail("rotl with rotation==bufferlen should succeed");
    }
    if (!expect_bytes(output, input, 4, "rotl full cycle")) {
      return 1;
    }
    std::puts("PASS: rotl full cycle");
    passed++;
  }

  // --- rotl failure: rotation > 16 ---
  {
    uint8_t input[4] = {0};
    uint8_t output[4] = {0};
    uint8_t ret = ntag424_rotl(input, output, 4, 17);
    if (ret != 0) {
      return fail("rotl should return 0 for rotation > 16");
    }
    std::puts("PASS: rotl overflow rejection");
    passed++;
  }

  // --- addpadding test ---
  {
    uint8_t buf[16] = {0x01, 0x02, 0x03};
    uint8_t result = ntag424_addpadding(3, 16, buf);
    if (result != 16) {
      return fail("addpadding(3, 16) should return 16");
    }
    if (buf[3] != 0x80) {
      return fail("addpadding should set 0x80 at first pad byte");
    }
    for (int i = 4; i < 16; i++) {
      if (buf[i] != 0x00) {
        return fail("addpadding should zero-fill remaining bytes");
      }
    }
    std::puts("PASS: addpadding");
    passed++;
  }

  // --- addpadding: already aligned ---
  {
    uint8_t buf[32] = {0};
    memset(buf, 0xAB, 16);
    uint8_t result = ntag424_addpadding(16, 16, buf);
    if (result != 32) {
      std::fprintf(stderr, "FAIL: addpadding(16, 16) should return 32, got %d\n",
                   result);
      return 1;
    }
    if (buf[16] != 0x80) {
      return fail("addpadding aligned should add full block with 0x80 prefix");
    }
    std::puts("PASS: addpadding aligned input");
    passed++;
  }

  // --- AES-128 CBC encrypt/decrypt round-trip (NIST SP 800-38A) ---
  {
    uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    uint8_t iv_enc[16]  = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t iv_dec[16]  = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t plaintext[16] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                             0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
    uint8_t expected_ct[16] = {0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
                               0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d};

    uint8_t ciphertext[16] = {0};
    uint8_t ret = ntag424_encrypt(key, iv_enc, 16, plaintext, ciphertext);
    if (ret != 1) {
      return fail("encrypt should return 1 on success");
    }
    if (!expect_bytes(ciphertext, expected_ct, 16, "AES-128-CBC encrypt")) {
      return 1;
    }

    uint8_t decrypted[16] = {0};
    ret = ntag424_decrypt(key, iv_dec, 16, ciphertext, decrypted);
    if (ret != 1) {
      return fail("decrypt should return 1 on success");
    }
    if (!expect_bytes(decrypted, plaintext, 16, "AES-128-CBC decrypt round-trip")) {
      return 1;
    }
    std::puts("PASS: AES-128-CBC encrypt/decrypt round-trip");
    passed++;
  }

  // --- AES-128 CMAC test (RFC 4493 Example 1: empty input) ---
  {
    uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    uint8_t expected_cmac[16] = {0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
                                 0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46};
    uint8_t cmac[16] = {0};

    ntag424_cmac(key, nullptr, 0, cmac);
    if (!expect_bytes(cmac, expected_cmac, 16, "AES-128-CMAC empty input")) {
      return 1;
    }
    std::puts("PASS: CMAC empty input (RFC 4493)");
    passed++;
  }

  // --- cmac_short test ---
  {
    uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    // cmac_short extracts odd-indexed bytes from full CMAC:
    // full CMAC: bb 1d 69 29 e9 59 37 28 7f a3 7d 12 9b 75 67 46
    // indices:    0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
    // odd indices (1,3,5,7,9,11,13,15): 1d 29 59 28 a3 12 75 46
    uint8_t expected_short[8] = {0x1d, 0x29, 0x59, 0x28, 0xa3, 0x12, 0x75, 0x46};
    uint8_t cmac_s[8] = {0};

    ntag424_cmac_short(key, nullptr, 0, cmac_s);
    if (!expect_bytes(cmac_s, expected_short, 8, "cmac_short empty input")) {
      return 1;
    }
    std::puts("PASS: cmac_short empty input");
    passed++;
  }

  // --- CRC32 test ---
  {
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc = ntag424_crc32(data, 4);
    // Verify it's non-zero and deterministic
    if (crc == 0) {
      return fail("CRC32 of non-empty data should not be zero");
    }
    uint32_t crc2 = ntag424_crc32(data, 4);
    if (crc != crc2) {
      return fail("CRC32 should be deterministic");
    }
    std::printf("PASS: CRC32 deterministic (0x%08X)\n", crc);
    passed++;
  }

  // --- encrypt/decrypt round-trip with zero IV (simpler overload) ---
  {
    uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t plaintext[32] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                             0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
                             0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                             0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60};
    uint8_t ciphertext[32] = {0};
    uint8_t decrypted[32] = {0};

    uint8_t ret = ntag424_encrypt(key, 32, plaintext, ciphertext);
    if (ret != 1) {
      return fail("encrypt(2-block) should return 1");
    }
    ret = ntag424_decrypt(key, 32, ciphertext, decrypted);
    if (ret != 1) {
      return fail("decrypt(2-block) should return 1");
    }
    if (!expect_bytes(decrypted, plaintext, 32,
                      "AES-128-CBC 2-block round-trip (zero IV)")) {
      return 1;
    }
    std::puts("PASS: AES-128-CBC 2-block round-trip (zero IV)");
    passed++;
  }

  // ntag424_random() is ESP32 hardware-dependent — skipped

  std::printf("\nPASS: all %d ntag424_crypto tests passed\n", passed);
  return 0;
}
