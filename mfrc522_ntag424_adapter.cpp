#include "mfrc522_ntag424_adapter.h"

#ifdef NTAG424DEBUG
#include <HardwareSerial.h>
#endif

uint8_t MFRC522_NTAG424_Adapter::transceive(const uint8_t *send,
                                            uint8_t sendLength,
                                            uint8_t *response,
                                            uint8_t responseMaxLength) {
  if (_reader == nullptr || send == nullptr || response == nullptr ||
      responseMaxLength == 0 || !_reader->isTagPresent()) {
    return 0;
  }

#ifdef NTAG424DEBUG
  Serial.print("[XCV-TX] len=");
  Serial.print(sendLength);
  Serial.print(" hex=");
  for (uint8_t i = 0; i < sendLength && i < 12; ++i) {
    if (send[i] < 0x10) Serial.print('0');
    Serial.print(send[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
#endif

  uint8_t responseLength = responseMaxLength;
  const MFRC522_I2C::StatusCode status = _reader->TCL_Transceive(
      &_reader->tag, send, sendLength, response, &responseLength);

#ifdef NTAG424DEBUG
  Serial.print("[XCV-RX] status=");
  Serial.print(static_cast<int>(status));
  Serial.print(" len=");
  Serial.print(responseLength);
  Serial.print(" hex=");
  for (uint8_t i = 0; i < responseLength && i < 12; ++i) {
    if (response[i] < 0x10) Serial.print('0');
    Serial.print(response[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
#endif

  if (status != MFRC522_I2C::STATUS_OK) {
    return 0;
  }

  return responseLength;
}

uint8_t MFRC522_NTAG424_Adapter::get_uid(uint8_t *uid, uint8_t uidMaxLength) {
  if (_reader == nullptr || uid == nullptr || uidMaxLength == 0) {
    return 0;
  }

  uint8_t uidLength = 0;
  _reader->getUID(uid, &uidLength);
  if (uidLength == 0 || uidLength > uidMaxLength) {
    return 0;
  }
  return uidLength;
}

bool MFRC522_NTAG424_Adapter::is_tag_present() {
  return _reader != nullptr && _reader->isTagPresent();
}
