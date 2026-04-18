#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../ntag424_core.h"

// ntag424_authresponse_TI provided by ntag424_core.cpp

class QueueReader : public NTAG424_Reader {
public:
  struct QueuedResponse {
    const uint8_t *data;
    uint8_t length;
  };

  uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                     uint8_t *response, uint8_t responseMaxLength) override {
    if (send != nullptr && sendLength <= sizeof(sent_frames[0]) &&
        sent_count < kMaxFrames) {
      memcpy(sent_frames[sent_count], send, sendLength);
      sent_lengths[sent_count] = sendLength;
      sent_count++;
    }

    if (response_index >= response_count) {
      return 0;
    }

    const QueuedResponse &queued = responses[response_index++];
    if (queued.length > responseMaxLength) {
      return 0;
    }

    memcpy(response, queued.data, queued.length);
    return queued.length;
  }

  uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override {
    (void)uid;
    (void)uidMaxLength;
    return 0;
  }

  bool is_tag_present() override { return true; }

  void queue_response(const uint8_t *data, uint8_t length) {
    if (response_count < kMaxFrames) {
      responses[response_count++] = {data, length};
    }
  }

  static constexpr uint8_t kMaxFrames = 8;
  QueuedResponse responses[kMaxFrames] = {};
  uint8_t response_count = 0;
  uint8_t response_index = 0;
  uint8_t sent_frames[kMaxFrames][80] = {};
  uint8_t sent_lengths[kMaxFrames] = {};
  uint8_t sent_count = 0;
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

}

int main() {
  int passed = 0;

  {
    QueueReader reader;
    const uint8_t short_response[1] = {0x90};
    reader.queue_response(short_response, sizeof(short_response));

    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    if (ntag424_ISOUpdateBinary(&reader, data, sizeof(data))) {
      return fail("ISOUpdateBinary should fail on short APDU response");
    }
    if (reader.sent_count != 1) {
      return fail("ISOUpdateBinary should stop after first failed chunk");
    }
    std::puts("PASS: ISOUpdateBinary rejects short response");
    passed++;
  }

  {
    QueueReader reader;
    const uint8_t ok_response[2] = {0x90, 0x00};
    reader.queue_response(ok_response, sizeof(ok_response));
    reader.queue_response(ok_response, sizeof(ok_response));
    reader.queue_response(ok_response, sizeof(ok_response));

    uint8_t data[60] = {0};
    for (uint8_t i = 0; i < sizeof(data); ++i) {
      data[i] = i;
    }

    if (!ntag424_ISOUpdateBinary(&reader, data, sizeof(data))) {
      return fail("ISOUpdateBinary should succeed when all chunks succeed");
    }
    if (reader.sent_count != 3) {
      return fail("ISOUpdateBinary should send three chunks for 60 bytes");
    }
    if (reader.sent_frames[0][0] != 0x00 || reader.sent_frames[0][1] != 0xD6 ||
        reader.sent_frames[0][2] != 0x00 || reader.sent_frames[0][3] != 0x00 ||
        reader.sent_frames[0][4] != 25) {
      return fail("ISOUpdateBinary first chunk APDU header mismatch");
    }
    if (reader.sent_frames[1][2] != 0x00 || reader.sent_frames[1][3] != 25 ||
        reader.sent_frames[1][4] != 25) {
      return fail("ISOUpdateBinary second chunk offset/length mismatch");
    }
    if (!expect_bytes(reader.sent_frames[1] + 5, data + 25, 25,
                       "ISOUpdateBinary second chunk payload")) {
      return 1;
    }
    if (reader.sent_frames[2][2] != 0x00 || reader.sent_frames[2][3] != 50 ||
        reader.sent_frames[2][4] != 10) {
      return fail("ISOUpdateBinary third chunk offset/length mismatch");
    }
    if (!expect_bytes(reader.sent_frames[2] + 5, data + 50, 10,
                      "ISOUpdateBinary third chunk payload")) {
      return 1;
    }
    std::puts("PASS: ISOUpdateBinary chunks and succeeds");
    passed++;
  }

  {
    QueueReader reader;
    ntag424_SessionType session = {};
    uint8_t key[16] = {0};
    if (ntag424_ISOAuthenticate(&reader, &session, key, 0) != 0) {
      return fail("ISOAuthenticate should fail when select/auth responses are missing");
    }
    std::puts("PASS: ISOAuthenticate fails cleanly without responses");
    passed++;
  }

  {
    QueueReader reader;
    ntag424_SessionType session = {};
    const uint8_t select_ok[2] = {0x90, 0x00};
    const uint8_t auth1_fail[2] = {0x91, 0xAE};
    reader.queue_response(select_ok, sizeof(select_ok));
    reader.queue_response(auth1_fail, sizeof(auth1_fail));

    uint8_t key[16] = {0};
    if (ntag424_ISOAuthenticate(&reader, &session, key, 0) != 0) {
      return fail("ISOAuthenticate should fail on non-91AF auth1 response");
    }
    if (reader.sent_count != 2) {
      return fail("ISOAuthenticate should send select + auth1 before failing");
    }
    if (reader.sent_frames[0][0] != 0x00 || reader.sent_frames[0][1] != 0xA4) {
      return fail("ISOAuthenticate should start with ISO SELECT FILE");
    }
    if (reader.sent_frames[1][0] != 0x00 || reader.sent_frames[1][1] != 0x86) {
      return fail("ISOAuthenticate should send ISO GENERAL AUTHENTICATE");
    }
    std::puts("PASS: ISOAuthenticate emits ISO auth APDUs");
    passed++;
  }

  {
    QueueReader reader;
    const uint8_t select_ok[2] = {0x90, 0x00};
    const uint8_t nlen_response[4] = {0x00, 0x03, 0x90, 0x00};
    const uint8_t data_response[5] = {0xD1, 0x01, 0x00, 0x90, 0x00};
    reader.queue_response(select_ok, sizeof(select_ok));
    reader.queue_response(select_ok, sizeof(select_ok));
    reader.queue_response(nlen_response, sizeof(nlen_response));
    reader.queue_response(data_response, sizeof(data_response));

    uint8_t buffer[8] = {0};
    const int16_t read = ntag424_ReadNDEFMessage(&reader, buffer, sizeof(buffer));
    if (read != 3) {
      std::fprintf(stderr,
                   "FAIL: ReadNDEFMessage should return 3 bytes, got %d\n",
                   read);
      return 1;
    }

    const uint8_t expected[3] = {0xD1, 0x01, 0x00};
    if (!expect_bytes(buffer, expected, sizeof(expected), "ReadNDEFMessage")) {
      return 1;
    }
    std::puts("PASS: ReadNDEFMessage reads payload");
    passed++;
  }

  {
    QueueReader reader;
    uint8_t buffer[56] = {0};
    if (ntag424_ReadSig(&reader, nullptr, buffer) != 0) {
      return fail("ReadSig should remain unsupported without multi-frame support");
    }
    if (reader.sent_count != 0) {
      return fail("ReadSig should not send frames in stubbed form");
    }
    std::puts("PASS: ReadSig remains explicit stub");
    passed++;
  }

  std::printf("\nPASS: all %d ntag424_core tests passed\n", passed);
  return 0;
}
