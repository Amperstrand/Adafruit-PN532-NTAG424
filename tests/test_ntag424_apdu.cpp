#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ntag424_apdu.h"

// --- Mock reader ---
class MockReader : public NTAG424_Reader {
public:
  uint8_t transceive_calls = 0;
  const uint8_t *canned_response = nullptr;
  uint8_t canned_length = 0;
  uint8_t last_send[80] = {0};
  uint8_t last_send_length = 0;

  uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                     uint8_t *response, uint8_t responseMaxLength) override {
    (void)responseMaxLength;
    transceive_calls++;
    if (sendLength > 0 && sendLength <= sizeof(last_send)) {
      memcpy(last_send, send, sendLength);
      last_send_length = sendLength;
    }
    if (canned_response != nullptr && canned_length > 0) {
      memcpy(response, canned_response, canned_length);
      return canned_length;
    }
    return 0;
  }

  uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override {
    (void)uid;
    (void)uidMaxLength;
    return 0;
  }

  bool is_tag_present() override { return true; }
};

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

  // --- ntag424_response_has_status: success ---
  {
    uint8_t response[4] = {0xDE, 0xAD, 0x90, 0x00};
    if (!ntag424_response_has_status(response, 4, 0x90, 0x00)) {
      return fail("response_has_status should detect 0x9000 at tail");
    }
    std::puts("PASS: response_has_status success");
    passed++;
  }

  // --- ntag424_response_has_status: wrong status ---
  {
    uint8_t response[4] = {0xDE, 0xAD, 0x91, 0xAE};
    if (ntag424_response_has_status(response, 4, 0x90, 0x00)) {
      return fail("response_has_status should reject wrong status");
    }
    std::puts("PASS: response_has_status wrong status rejected");
    passed++;
  }

  // --- ntag424_response_has_status: too short ---
  {
    uint8_t response[1] = {0x90};
    if (ntag424_response_has_status(response, 1, 0x90, 0x00)) {
      return fail("response_has_status should reject 1-byte response");
    }
    std::puts("PASS: response_has_status too short rejected");
    passed++;
  }

  // --- ntag424_response_has_status: null ---
  {
    if (ntag424_response_has_status(nullptr, 4, 0x90, 0x00)) {
      return fail("response_has_status should reject null response");
    }
    std::puts("PASS: response_has_status null rejected");
    passed++;
  }

  // --- ntag424_plain_command_succeeded: success ---
  {
    uint8_t response[6] = {0x01, 0x02, 0x03, 0x04, 0x90, 0x00};
    if (!ntag424_plain_command_succeeded(response, 6)) {
      return fail("plain_command_succeeded should return true for 0x9000");
    }
    std::puts("PASS: plain_command_succeeded success");
    passed++;
  }

  // --- ntag424_plain_command_succeeded: failure ---
  {
    uint8_t response[4] = {0x00, 0x00, 0x91, 0xAE};
    if (ntag424_plain_command_succeeded(response, 4)) {
      return fail("plain_command_succeeded should return false for non-0x9000");
    }
    std::puts("PASS: plain_command_succeeded failure");
    passed++;
  }

  // --- ntag424_copy_response_data_if_status: success with data ---
  {
    uint8_t response[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0x90, 0x00};
    uint8_t buffer[4] = {0};
    uint8_t len = ntag424_copy_response_data_if_status(response, 6, 0x90, 0x00, buffer);
    if (len != 4) {
      std::fprintf(stderr, "FAIL: copy_response_data_if_status should return 4, got %d\n", len);
      return 1;
    }
    uint8_t expected[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    if (!expect_bytes(buffer, expected, 4, "copy_response_data_if_status")) {
      return 1;
    }
    std::puts("PASS: copy_response_data_if_status with data");
    passed++;
  }

  // --- ntag424_copy_response_data_if_status: wrong status ---
  {
    uint8_t response[4] = {0xAA, 0xBB, 0x91, 0xAE};
    uint8_t buffer[4] = {0};
    uint8_t len = ntag424_copy_response_data_if_status(response, 4, 0x90, 0x00, buffer);
    if (len != 0) {
      return fail("copy_response_data_if_status should return 0 on wrong status");
    }
    std::puts("PASS: copy_response_data_if_status wrong status");
    passed++;
  }

  // --- ntag424_copy_response_data_if_status: status only (no data) ---
  {
    uint8_t response[2] = {0x90, 0x00};
    uint8_t buffer[4] = {0xFF};
    uint8_t len = ntag424_copy_response_data_if_status(response, 2, 0x90, 0x00, buffer);
    if (len != 0) {
      return fail("copy_response_data_if_status should return 0 when no data");
    }
    std::puts("PASS: copy_response_data_if_status status only");
    passed++;
  }

  // --- ntag424_plain_status_ok: delegates to response_has_status ---
  {
    uint8_t response[4] = {0x00, 0x00, 0x91, 0x00};
    if (!ntag424_plain_status_ok(response, 4, 0x91, 0x00)) {
      return fail("plain_status_ok should detect 0x9100");
    }
    std::puts("PASS: plain_status_ok");
    passed++;
  }

  // --- ntag424_build_apdu: PLAIN mode, no data ---
  {
    uint8_t apdu[80] = {0};
    uint8_t len = ntag424_build_apdu(
        0x90,       // cla
        0x30,       // ins (GET VERSION)
        0x00, 0x00, // p1, p2
        nullptr, 0, // cmd_header
        nullptr, 0, // cmd_data
        0x00,       // le
        0x00,       // comm_mode = PLAIN
        nullptr,    // session
        apdu);
    if (len == 0) {
      return fail("build_apdu PLAIN no-data should succeed");
    }
    // Expected: CLA INS P1 P2 Lc Le = 90 30 00 00 00 00
    uint8_t expected[6] = {0x90, 0x30, 0x00, 0x00, 0x00, 0x00};
    if (!expect_bytes(apdu, expected, 6, "PLAIN no-data APDU")) {
      return 1;
    }
    std::puts("PASS: build_apdu PLAIN no data");
    passed++;
  }

  // --- ntag424_build_apdu: PLAIN mode, with data ---
  {
    uint8_t cmd_data[4] = {0x02, 0x00, 0x01, 0x00};
    uint8_t apdu[80] = {0};
    uint8_t len = ntag424_build_apdu(
        0x90,       // cla
        0x30,       // ins
        0x00, 0x00, // p1, p2
        nullptr, 0, // cmd_header
        cmd_data, 4,// cmd_data
        0x00,       // le
        0x00,       // comm_mode = PLAIN
        nullptr,    // session
        apdu);
    if (len == 0) {
      return fail("build_apdu PLAIN with-data should succeed");
    }
    // Expected: CLA INS P1 P2 Lc=04 data[4] Le=00
    uint8_t expected[11] = {0x90, 0x30, 0x00, 0x00, 0x04,
                            0x02, 0x00, 0x01, 0x00, 0x00};
    if (!expect_bytes(apdu, expected, 10, "PLAIN with-data APDU")) {
      return 1;
    }
    std::puts("PASS: build_apdu PLAIN with data");
    passed++;
  }

  // --- ntag424_build_apdu: PLAIN mode, CLA 0x00 (ISO SELECT) ---
  {
    uint8_t cmd_data[2] = {0xE1, 0x03};
    uint8_t apdu[80] = {0};
    uint8_t len = ntag424_build_apdu(
        0x00,       // cla (ISO)
        0xA4,       // ins (SELECT FILE)
        0x04, 0x00, // p1, p2
        nullptr, 0, // cmd_header
        cmd_data, 2,// cmd_data
        0x00,       // le
        0x00,       // comm_mode = PLAIN
        nullptr,    // session
        apdu);
    if (len == 0) {
      return fail("build_apdu ISO SELECT should succeed");
    }
    // Expected: CLA INS P1 P2 Lc=02 data[2] Le=00
    uint8_t expected[9] = {0x00, 0xA4, 0x04, 0x00, 0x02,
                           0xE1, 0x03, 0x00};
    if (!expect_bytes(apdu, expected, 8, "ISO SELECT APDU")) {
      return 1;
    }
    std::puts("PASS: build_apdu ISO SELECT");
    passed++;
  }

  // --- ntag424_build_apdu: null output returns 0 ---
  {
    uint8_t len = ntag424_build_apdu(
        0x90, 0x30, 0x00, 0x00,
        nullptr, 0, nullptr, 0,
        0x00, 0x00, nullptr, nullptr);
    if (len != 0) {
      return fail("build_apdu with null output should return 0");
    }
    std::puts("PASS: build_apdu null output");
    passed++;
  }

  // --- ntag424_build_apdu: MAC mode without session returns 0 ---
  {
    uint8_t apdu[80] = {0};
    uint8_t len = ntag424_build_apdu(
        0x90, 0x30, 0x00, 0x00,
        nullptr, 0, nullptr, 0,
        0x00, 0x01, // comm_mode = MAC
        nullptr,    // session (null)
        apdu);
    if (len != 0) {
      return fail("build_apdu MAC without session should return 0");
    }
    std::puts("PASS: build_apdu MAC without session");
    passed++;
  }

  // --- ntag424_build_apdu: FULL mode without session returns 0 ---
  {
    uint8_t apdu[80] = {0};
    uint8_t len = ntag424_build_apdu(
        0x90, 0x30, 0x00, 0x00,
        nullptr, 0, nullptr, 0,
        0x00, 0x02, // comm_mode = FULL
        nullptr,    // session (null)
        apdu);
    if (len != 0) {
      return fail("build_apdu FULL without session should return 0");
    }
    std::puts("PASS: build_apdu FULL without session");
    passed++;
  }

  // --- ntag424_process_response: PLAIN mode pass-through ---
  {
    uint8_t response[4] = {0xAA, 0xBB, 0x90, 0x00};
    uint8_t processed[4] = {0};
    uint8_t len =
        ntag424_process_response(response, 4, 0x00, nullptr, processed);
    if (len != 4) {
      std::fprintf(stderr, "FAIL: process_response PLAIN should return 4, got %d\n", len);
      return 1;
    }
    if (!expect_bytes(processed, response, 4, "process_response PLAIN")) {
      return 1;
    }
    std::puts("PASS: process_response PLAIN pass-through");
    passed++;
  }

  // --- ntag424_process_response: null inputs return 0 ---
  {
    uint8_t response[4] = {0};
    uint8_t processed[4] = {0};
    uint8_t len = ntag424_process_response(nullptr, 4, 0x00, nullptr, processed);
    if (len != 0) {
      return fail("process_response null response should return 0");
    }
    len = ntag424_process_response(response, 4, 0x00, nullptr, nullptr);
    if (len != 0) {
      return fail("process_response null output should return 0");
    }
    std::puts("PASS: process_response null inputs");
    passed++;
  }

  // --- ntag424_iso_select_file with mock reader ---
  {
    MockReader reader;
    uint8_t canned_response[2] = {0x90, 0x00};
    reader.canned_response = canned_response;
    reader.canned_length = 2;

    uint8_t cmd_data[2] = {0xE1, 0x03};
    bool result = ntag424_iso_select_file(&reader, 0x04, cmd_data, 2, nullptr);
    if (!result) {
      return fail("iso_select_file should succeed with 0x9000 response");
    }
    if (reader.transceive_calls != 1) {
      return fail("iso_select_file should call transceive exactly once");
    }
    // Verify the sent APDU starts with ISO CLA 0x00, INS 0xA4
    if (reader.last_send[0] != 0x00) {
      return fail("iso_select_file should send CLA=0x00");
    }
    if (reader.last_send[1] != 0xA4) {
      return fail("iso_select_file should send INS=0xA4");
    }
    std::puts("PASS: iso_select_file with mock reader");
    passed++;
  }

  // --- ntag424_iso_select_file failure response ---
  {
    MockReader reader;
    uint8_t canned_response[2] = {0x91, 0xAE};
    reader.canned_response = canned_response;
    reader.canned_length = 2;

    uint8_t cmd_data[2] = {0xE1, 0x03};
    bool result = ntag424_iso_select_file(&reader, 0x04, cmd_data, 2, nullptr);
    if (result) {
      return fail("iso_select_file should fail with non-0x9000 response");
    }
    std::puts("PASS: iso_select_file failure response");
    passed++;
  }

  // --- ntag424_iso_select_file null reader ---
  {
    uint8_t cmd_data[2] = {0xE1, 0x03};
    bool result = ntag424_iso_select_file(nullptr, 0x04, cmd_data, 2, nullptr);
    if (result) {
      return fail("iso_select_file with null reader should fail");
    }
    std::puts("PASS: iso_select_file null reader");
    passed++;
  }

  std::printf("\nPASS: all %d ntag424_apdu tests passed\n", passed);
  return 0;
}
