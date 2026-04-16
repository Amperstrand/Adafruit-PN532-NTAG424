#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "../Adafruit_PN532_NTAG424.h"
#undef private

uint8_t ntag424_authresponse_TI[NTAG424_AUTHRESPONSE_TI_SIZE] = {0x00, 0x00,
                                                                 0x00, 0x00};

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
  int passed = 0;

  {
    Adafruit_PN532 pn532(5);
    pn532._uidLen = 4;
    pn532._uid[0] = 0x04;
    pn532._uid[1] = 0xA1;
    pn532._uid[2] = 0xB2;
    pn532._uid[3] = 0xC3;
    pn532._inListedTag = 1;

    uint8_t uid[7] = {0};
    const uint8_t uid_len = pn532._ntag424_adapter.get_uid(uid, sizeof(uid));
    const uint8_t expected_uid[4] = {0x04, 0xA1, 0xB2, 0xC3};
    if (uid_len != 4) {
      return fail("adapter get_uid should return the stored UID length");
    }
    if (!expect_bytes(uid, expected_uid, sizeof(expected_uid),
                      "adapter get_uid")) {
      return 1;
    }
    if (!pn532._ntag424_adapter.is_tag_present()) {
      return fail("adapter should report tag present when PN532 has a listed tag");
    }
    std::puts("PASS: public header adapter getters work");
    passed++;
  }

  {
    Adafruit_PN532 pn532(5);
    pn532._uidLen = 5;
    pn532._uid[0] = 0x04;
    pn532._uid[1] = 0x11;
    pn532._uid[2] = 0x22;
    pn532._uid[3] = 0x33;
    pn532._uid[4] = 0x44;

    uint8_t uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t uid_len = pn532._ntag424_adapter.get_uid(uid, sizeof(uid));
    if (uid_len != 0) {
      return fail("adapter get_uid should fail when caller buffer is too small");
    }
    std::puts("PASS: public header adapter rejects short UID buffer");
    passed++;
  }

  {
    PN532_NTAG424_Adapter adapter(nullptr);
    uint8_t uid[7] = {0};
    if (adapter.get_uid(uid, sizeof(uid)) != 0) {
      return fail("adapter get_uid should reject null PN532 instance");
    }
    if (adapter.is_tag_present()) {
      return fail("adapter should reject null PN532 instance for tag presence");
    }
    uint8_t response[8] = {0};
    const uint8_t send[2] = {0x90, 0x60};
    if (adapter.transceive(send, sizeof(send), response, sizeof(response)) != 0) {
      return fail("adapter transceive should reject null PN532 instance");
    }
    std::puts("PASS: public header adapter null guards work");
    passed++;
  }

  std::printf("\nPASS: all %d public-header tests passed\n", passed);
  return 0;
}
