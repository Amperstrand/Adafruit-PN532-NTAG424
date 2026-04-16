#include "pn532_ntag424_adapter.h"

#include "Adafruit_PN532_NTAG424.h"

#include <string.h>

uint8_t PN532_NTAG424_Adapter::transceive(const uint8_t *send,
                                          uint8_t sendLength,
                                          uint8_t *response,
                                          uint8_t responseMaxLength) {
  (void)responseMaxLength;

  uint8_t pn532_frame[sendLength + 2];
  pn532_frame[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_frame[1] = 0x01;
  memcpy(pn532_frame + 2, send, sendLength);

  if (!_pn532->sendCommandCheckAck(pn532_frame, sendLength + 2)) {
    return 0;
  }

  uint8_t resp_buf[64];
  _pn532->readdata(resp_buf, sizeof(resp_buf));

  uint8_t response_length = resp_buf[3] - 3;
  memcpy(response, resp_buf + 8, response_length);
  return response_length;
}

uint8_t PN532_NTAG424_Adapter::get_uid(uint8_t *uid, uint8_t uidMaxLength) {
  uint8_t uid_buffer[7];
  uint8_t uid_length = 0;
  _pn532->getUID(uid_buffer, &uid_length);

  if (uid_length > uidMaxLength) {
    return 0;
  }

  memcpy(uid, uid_buffer, uid_length);
  return uid_length;
}

bool PN532_NTAG424_Adapter::is_tag_present() { return _pn532->isTagPresent(); }
