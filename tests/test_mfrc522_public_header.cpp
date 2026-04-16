#include <cstdint>
#include <cstdio>
#include <cstring>

#define protected public
#define private public
#include "../MFRC522_NTAG424.h"
#undef private
#undef protected

// ntag424_authresponse_TI provided by ntag424_core.cpp

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
  return false;
}

}

int main() {
  int passed = 0;

  {
    MFRC522_NTAG424 reader;
    reader.uid.size = 7;
    reader.uid.uidByte[0] = 0x04;
    reader.uid.uidByte[1] = 0x11;
    reader.uid.uidByte[2] = 0x22;
    reader.uid.uidByte[3] = 0x33;
    reader.uid.uidByte[4] = 0x44;
    reader.uid.uidByte[5] = 0x55;
    reader.uid.uidByte[6] = 0x66;
    reader.tag.uid = reader.uid;
    reader.tag.ats.tc1.supportsCID = false;
    reader._tag_selected = true;

    uint8_t uid[10] = {0};
    const uint8_t uidLength = reader._ntag424_adapter.get_uid(uid, sizeof(uid));
    const uint8_t expectedUid[7] = {0x04, 0x11, 0x22, 0x33,
                                    0x44, 0x55, 0x66};
    if (uidLength != sizeof(expectedUid)) {
      return fail("MFRC522 adapter should report the stored UID length");
    }
    if (!expect_bytes(uid, expectedUid, sizeof(expectedUid), "MFRC522 UID")) {
      return 1;
    }
    if (!reader._ntag424_adapter.is_tag_present()) {
      return fail("MFRC522 adapter should report tag present when selected");
    }
    std::puts("PASS: MFRC522 public header adapter getters work");
    passed++;
  }

  {
    MFRC522_NTAG424 reader;
    reader.uid.size = 5;
    reader.tag.uid = reader.uid;
    reader.tag.ats.tc1.supportsCID = false;
    reader._tag_selected = true;

    uint8_t uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    if (reader._ntag424_adapter.get_uid(uid, sizeof(uid)) != 0) {
      return fail("MFRC522 adapter should reject short UID buffer");
    }
    std::puts("PASS: MFRC522 public header rejects short UID buffer");
    passed++;
  }

  {
    MFRC522_NTAG424_Adapter adapter(nullptr);
    uint8_t uid[7] = {0};
    if (adapter.get_uid(uid, sizeof(uid)) != 0) {
      return fail("MFRC522 adapter should reject null reader instance");
    }
    if (adapter.is_tag_present()) {
      return fail("MFRC522 adapter should reject null reader tag presence");
    }
    uint8_t response[8] = {0};
    const uint8_t send[2] = {0x90, 0x60};
    if (adapter.transceive(send, sizeof(send), response, sizeof(response)) != 0) {
      return fail("MFRC522 adapter should reject null reader transceive");
    }
    std::puts("PASS: MFRC522 public header null guards work");
    passed++;
  }

  std::printf("\nPASS: all %d MFRC522 public-header tests passed\n", passed);
  return 0;
}
