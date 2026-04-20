#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ntag424_core.h"
#include "mock_reader.h"

namespace {

constexpr uint8_t kDefaultKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};

constexpr uint8_t kSelectApplicationApdu[13] = {
    0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76,
    0x00, 0x00, 0x85, 0x01, 0x01, 0x00};

constexpr uint8_t kNextFrameApdu[5] = {0x90, 0xAF, 0x00, 0x00, 0x00};

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

bool expect_frame(const ScriptedMockReader &reader, size_t index,
                  const uint8_t *expected, size_t len, const char *label) {
  const std::vector<std::vector<uint8_t>> &frames = reader.sent_frames();
  if (index >= frames.size()) {
    std::fprintf(stderr, "FAIL: %s missing frame %zu\n", label, index);
    return false;
  }
  if (frames[index].size() != len) {
    std::fprintf(stderr, "FAIL: %s frame length mismatch (expected %zu got %zu)\n",
                 label, len, frames[index].size());
    return false;
  }
  return expect_bytes(frames[index].data(), expected, len, label);
}

void queue_get_version_sequence(ScriptedMockReader &reader) {
  const uint8_t frame1[] = {0x04, 0x04, 0x02, 0x01, 0x00,
                            0x11, 0x05, 0x91, 0xAF};
  const uint8_t frame2[] = {0x04, 0x04, 0x02, 0x01, 0x00,
                            0x11, 0x05, 0x91, 0xAF};
  const uint8_t frame3[] = {0x04, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02,
                            0x11, 0x22, 0x33, 0x44, 0x55, 0x12, 0x24,
                            0x91, 0x00};
  reader.queue_response(frame1, sizeof(frame1));
  reader.queue_response(frame2, sizeof(frame2));
  reader.queue_response(frame3, sizeof(frame3));
}

void queue_successful_auth_sequence(ScriptedMockReader &reader, uint8_t keyno,
                                    const uint8_t *key, uint8_t *expected_ti,
                                    uint8_t *expected_rndb) {
  const uint8_t select_ok[] = {0x90, 0x00};
  const uint8_t rndb[16] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                            0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
  const uint8_t ti[4] = {0x11, 0x22, 0x33, 0x44};
  const uint8_t pdcap2[6] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  const uint8_t pcdcap2[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t auth1_response[18] = {0};
  uint8_t auth2_plain[32] = {0};
  uint8_t auth2_response[34] = {0};
  uint8_t rnda_rot[16] = {0};
  uint8_t deterministic_rnda[16];
  std::memset(deterministic_rnda, 0x04, sizeof(deterministic_rnda));

  ntag424_encrypt(const_cast<uint8_t *>(key), 16, const_cast<uint8_t *>(rndb),
                  auth1_response);
  auth1_response[16] = 0x91;
  auth1_response[17] = 0xAF;

  ntag424_rotl(deterministic_rnda, rnda_rot, sizeof(rnda_rot), 1);
  std::memcpy(auth2_plain, ti, sizeof(ti));
  std::memcpy(auth2_plain + 4, rnda_rot, sizeof(rnda_rot));
  std::memcpy(auth2_plain + 20, pdcap2, sizeof(pdcap2));
  std::memcpy(auth2_plain + 26, pcdcap2, sizeof(pcdcap2));
  ntag424_encrypt(const_cast<uint8_t *>(key), sizeof(auth2_plain), auth2_plain,
                  auth2_response);
  auth2_response[32] = 0x91;
  auth2_response[33] = 0x00;

  reader.queue_response(select_ok, sizeof(select_ok));
  const uint8_t auth1_header[] = {0x90, NTAG424_CMD_NEXTFRAME};
  (void)auth1_header;
  (void)keyno;
  reader.queue_response(auth1_response, sizeof(auth1_response));
  reader.queue_response(auth2_response, sizeof(auth2_response));

  if (expected_ti != nullptr) {
    std::memcpy(expected_ti, ti, sizeof(ti));
  }
  if (expected_rndb != nullptr) {
    std::memcpy(expected_rndb, rndb, sizeof(rndb));
  }
}

}

int main() {
  int passed = 0;

  {
    ScriptedMockReader reader;
    const uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    ntag424_VersionInfoType version = {};
    reader.set_uid(uid, sizeof(uid));
    reader.set_tag_present(true);
    queue_get_version_sequence(reader);

    uint8_t read_uid[7] = {0};
    if (reader.get_uid(read_uid, sizeof(read_uid)) != sizeof(uid)) {
      return fail("mock reader should return fixed UID");
    }
    if (!expect_bytes(read_uid, uid, sizeof(uid), "mock UID")) {
      return 1;
    }
    if (!reader.is_tag_present()) {
      return fail("mock reader should report tag present");
    }
    if (ntag424_GetVersion(&reader, &version) != 1) {
      return fail("GetVersion should succeed with scripted frames");
    }
    if (version.VendorID != 0x04 || version.HWType != 0x04 ||
        version.SWType != 0x04 || version.YearProd != 0x24) {
      return fail("GetVersion should parse NTAG424 version fields");
    }
    const uint8_t expected_uid_from_version[7] = {0x04, 0xDE, 0xAD, 0xBE,
                                                  0xEF, 0x01, 0x02};
    if (!expect_bytes(version.UID, expected_uid_from_version,
                      sizeof(expected_uid_from_version),
                      "GetVersion UID parse")) {
      return 1;
    }
    std::puts("PASS: successful GetVersion");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_SessionType session = {};
    uint8_t expected_ti[4] = {0};
    uint8_t expected_rndb[16] = {0};
    queue_successful_auth_sequence(reader, 0x00,
                                   const_cast<uint8_t *>(kDefaultKey),
                                   expected_ti, expected_rndb);

    if (ntag424_Authenticate(&reader, &session,
                             const_cast<uint8_t *>(kDefaultKey), 0x00,
                             0x71) != 1) {
      return fail("Authenticate should complete full EV2 handshake");
    }
    if (!session.authenticated || session.cmd_counter != 0) {
      return fail("Authenticate should establish authenticated session");
    }
    if (!expect_bytes(session.TI, expected_ti, sizeof(expected_ti),
                      "auth TI")) {
      return 1;
    }
    if (!expect_frame(reader, 0, kSelectApplicationApdu,
                      sizeof(kSelectApplicationApdu), "auth select")) {
      return 1;
    }
    const uint8_t expected_auth1[] = {0x90, 0x71, 0x00, 0x00, 0x05, 0x00,
                                      0x03, 0x00, 0x00, 0x00, 0x00};
    if (!expect_frame(reader, 1, expected_auth1, sizeof(expected_auth1),
                      "auth first frame")) {
      return 1;
    }
    const std::vector<std::vector<uint8_t>> &frames = reader.sent_frames();
    if (frames.size() != 3 || frames[2].size() != 38 || frames[2][0] != 0x90 ||
        frames[2][1] != 0xAF || frames[2][4] != 0x20) {
      return fail("Authenticate should send AF continuation frame");
    }
    if (session.session_key_enc[0] == 0x00 && session.session_key_mac[0] == 0x00) {
      return fail("Authenticate should derive non-zero session keys");
    }
    (void)expected_rndb;
    std::puts("PASS: successful auth handshake");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_VersionInfoType version = {};
    const uint8_t frame1[] = {0x04, 0x04, 0x02, 0x01, 0x00,
                              0x11, 0x05, 0x91, 0xAF};
    const uint8_t frame2[] = {0x04, 0x04, 0x02, 0x01, 0x00,
                              0x11, 0x05, 0x91, 0xAF};
    const uint8_t frame3[] = {0x04, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60,
                              0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x21, 0x26,
                              0x91, 0x00};
    reader.queue_response(frame1, sizeof(frame1));
    reader.queue_wtx(0x05);
    reader.queue_response(frame2, sizeof(frame2));
    reader.queue_response(frame3, sizeof(frame3));

    if (ntag424_GetVersion(&reader, &version) != 1) {
      return fail("GetVersion should succeed across WTX pause");
    }
    if (reader.wtx_count() != 1 || reader.transceive_call_count() != 3) {
      return fail("WTX should be absorbed by mock transceiver");
    }
    std::puts("PASS: WTX handled");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_SessionType session = {};
    const uint8_t select_ok[] = {0x90, 0x00};
    const uint8_t auth_fail[] = {0x91, 0xAE};
    reader.queue_response(select_ok, sizeof(select_ok));
    reader.queue_response(auth_fail, sizeof(auth_fail));

    if (ntag424_Authenticate(&reader, &session,
                             const_cast<uint8_t *>(kDefaultKey), 0x00,
                             0x71) != 0) {
      return fail("Authenticate should fail on 91AE");
    }
    if (session.authenticated) {
      return fail("Authenticate failure should not mark session authenticated");
    }
    std::puts("PASS: auth failure 91AE");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_SessionType session = {};
    uint8_t key_version = 0x00;
    uint8_t expected_ti[4] = {0};
    uint8_t expected_rndb[16] = {0};
    const uint8_t integrity_error[] = {0x91, 0x1E};
    queue_successful_auth_sequence(reader, 0x00,
                                   const_cast<uint8_t *>(kDefaultKey),
                                   expected_ti, expected_rndb);
    reader.queue_response(integrity_error, sizeof(integrity_error));

    if (ntag424_Authenticate(&reader, &session,
                             const_cast<uint8_t *>(kDefaultKey), 0x00,
                             0x71) != 1) {
      return fail("precondition auth should succeed before integrity test");
    }
    if (ntag424_GetKeyVersion(&reader, &session, 0x00, &key_version)) {
      return fail("GetKeyVersion should fail on 911E integrity error");
    }
    if (session.cmd_counter != 1) {
      return fail("plain command after auth should advance cmd_counter");
    }
    std::puts("PASS: cmd_counter desync integrity error detected");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_VersionInfoType version = {};
    queue_get_version_sequence(reader);

    if (ntag424_GetVersion(&reader, &version) != 1) {
      return fail("GetVersion chained response should succeed");
    }
    if (!expect_frame(reader, 1, kNextFrameApdu, sizeof(kNextFrameApdu),
                      "first additional frame request")) {
      return 1;
    }
    if (!expect_frame(reader, 2, kNextFrameApdu, sizeof(kNextFrameApdu),
                      "second additional frame request")) {
      return 1;
    }
    if (version.SWProtocol != 0x05 || version.CWProd != 0x12) {
      return fail("chained GetVersion should reassemble later-frame fields");
    }
    std::puts("PASS: chained response additional frame");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_VersionInfoType version = {};
    reader.set_fail_on_call(2);
    queue_get_version_sequence(reader);

    if (ntag424_GetVersion(&reader, &version) != 0) {
      return fail("GetVersion should fail when transceive is injected to fail");
    }
    if (reader.transceive_call_count() != 2) {
      return fail("error injection should fail exactly on configured call");
    }
    std::puts("PASS: error injection on second call");
    passed++;
  }

  {
    ScriptedMockReader reader;
    ntag424_SessionType session = {};
    uint8_t key_version = 0x00;
    uint8_t expected_ti[4] = {0};
    uint8_t expected_rndb[16] = {0};
    const uint8_t length_error[] = {0x91, 0x7E};
    queue_successful_auth_sequence(reader, 0x00,
                                   const_cast<uint8_t *>(kDefaultKey),
                                   expected_ti, expected_rndb);
    reader.queue_response(length_error, sizeof(length_error));

    if (ntag424_Authenticate(&reader, &session,
                             const_cast<uint8_t *>(kDefaultKey), 0x00,
                             0x71) != 1) {
      return fail("precondition auth should succeed before length-error test");
    }
    if (ntag424_GetKeyVersion(&reader, &session, 0x00, &key_version)) {
      return fail("GetKeyVersion should fail on 917E during auth session");
    }
    if (reader.scripted_steps_remaining() != 0) {
      return fail("all scripted responses should be consumed");
    }
    std::puts("PASS: GetKeyVersion length error during auth session");
    passed++;
  }

  std::printf("\nPASS: all %d mock transceiver tests passed\n", passed);
  return 0;
}
