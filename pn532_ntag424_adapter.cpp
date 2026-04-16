#include "pn532_ntag424_adapter.h"

#include "Adafruit_PN532_NTAG424.h"

#include <string.h>

namespace {

constexpr uint8_t kMaxNtag424ApduLength = 80;
constexpr uint8_t kMaxPn532FrameLength = kMaxNtag424ApduLength + 2;

}

uint8_t PN532_NTAG424_Adapter::transceive(const uint8_t *send,
                                          uint8_t sendLength,
                                          uint8_t *response,
                                          uint8_t responseMaxLength) {
  if (_pn532 == nullptr || send == nullptr || response == nullptr ||
      responseMaxLength == 0 || sendLength > kMaxNtag424ApduLength) {
    return 0;
  }

  uint8_t pn532_frame[kMaxPn532FrameLength] = {0};
  pn532_frame[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_frame[1] = 0x01;
  memcpy(pn532_frame + 2, send, sendLength);

  if (!_pn532->sendCommandCheckAck(pn532_frame, sendLength + 2)) {
    return 0;
  }

  uint8_t resp_buf[64];
  _pn532->readdata(resp_buf, sizeof(resp_buf));

  if (resp_buf[0] != 0x00 || resp_buf[1] != 0x00 || resp_buf[2] != 0xFF) {
    return 0;
  }

  const uint8_t frame_length = resp_buf[3];
  if (resp_buf[4] != static_cast<uint8_t>(~frame_length + 1) ||
      frame_length < 3 || resp_buf[5] != PN532_PN532TOHOST ||
      resp_buf[6] != PN532_RESPONSE_INDATAEXCHANGE ||
      (resp_buf[7] & 0x3F) != 0) {
    return 0;
  }

  const uint8_t payload_length = frame_length - 3;
  const uint8_t available_payload = sizeof(resp_buf) - 10;
  if (payload_length > available_payload) {
    return 0;
  }

  const uint8_t copied_length =
      payload_length > responseMaxLength ? responseMaxLength : payload_length;
  memcpy(response, resp_buf + 8, copied_length);
  return copied_length;
}

uint8_t PN532_NTAG424_Adapter::get_uid(uint8_t *uid, uint8_t uidMaxLength) {
  if (_pn532 == nullptr || uid == nullptr || uidMaxLength == 0) {
    return 0;
  }

  uint8_t uid_buffer[7];
  uint8_t uid_length = 0;
  _pn532->getUID(uid_buffer, &uid_length);

  if (uid_length > uidMaxLength) {
    return 0;
  }

  memcpy(uid, uid_buffer, uid_length);
  return uid_length;
}

bool PN532_NTAG424_Adapter::is_tag_present() {
  return _pn532 != nullptr && _pn532->isTagPresent();
}
