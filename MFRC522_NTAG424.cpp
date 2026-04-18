#include "MFRC522_NTAG424.h"

#if defined(ARDUINO)
#include <Wire.h>
#endif

MFRC522_NTAG424::MFRC522_NTAG424(uint8_t i2cAddress, TwoWire *wire)
    : MFRC522_I2C_Extended(i2cAddress, wire), _ntag424_adapter(this) {
  attach_reader(&_ntag424_adapter);
}

bool MFRC522_NTAG424::begin(bool initWire) {
  if (_wire == nullptr) {
    return false;
  }
  if (initWire) {
    _wire->begin();
  }

  uint8_t version = 0;
  const bool pre_read_ok = PCD_ReadRegisterChecked(VersionReg, &version);
  if (!pre_read_ok || version == 0x00 || version == 0xFF) {
    return false;
  }
  const bool init_ok = PCD_Init();
  if (!init_ok) {
    const bool fallback_ok = PCD_ConfigureDefaults();
    if (!fallback_ok) {
      return false;
    }
  }
  const bool post_read_ok = PCD_ReadRegisterChecked(VersionReg, &version);
  if (!post_read_ok) {
    return false;
  }
  return version != 0x00 && version != 0xFF;
}

bool MFRC522_NTAG424::begin(int sda, int scl, uint32_t frequency) {
  if (_wire == nullptr) {
    return false;
  }
  _wire->begin(sda, scl, frequency);
  return begin(false);
}

bool MFRC522_NTAG424::readPassiveTargetID(uint8_t *uidBuffer,
                                           uint8_t *uidLength) {
  if (uidLength == nullptr) {
    return false;
  }

  if (isTagPresent()) {
    if (tag.ats.size > 0) {
      TCL_Deselect(&tag);
    }
    PICC_HaltA();
    clearTag();
    delay(5);
  }

  if (!PICC_IsNewCardPresent() || !PICC_ReadCardSerial()) {
    *uidLength = 0;
    return false;
  }

  getUID(uidBuffer, uidLength);
  return *uidLength != 0;
}

uint8_t MFRC522_NTAG424::ntag424_apdu_send(
    uint8_t *cla, uint8_t *ins, uint8_t *p1, uint8_t *p2, uint8_t *cmd_header,
    uint8_t cmd_header_length, uint8_t *cmd_data, uint8_t cmd_data_length,
    uint8_t le, uint8_t comm_mode, uint8_t *response, uint8_t response_le) {
  return NTAG424_Handler::ntag424_apdu_send(
      cla, ins, p1, p2, cmd_header, cmd_header_length, cmd_data,
      cmd_data_length, le, comm_mode, response, response_le);
}

uint32_t MFRC522_NTAG424::ntag424_crc32(uint8_t *data, uint8_t datalength) {
  return NTAG424_Handler::ntag424_crc32(data, datalength);
}

uint8_t MFRC522_NTAG424::ntag424_addpadding(uint8_t inputlength,
                                            uint8_t paddinglength,
                                            uint8_t *buffer) {
  return NTAG424_Handler::ntag424_addpadding(inputlength, paddinglength,
                                             buffer);
}

uint8_t MFRC522_NTAG424::ntag424_encrypt(uint8_t *key, uint8_t length,
                                         uint8_t *input, uint8_t *output) {
  return NTAG424_Handler::ntag424_encrypt(key, length, input, output);
}

uint8_t MFRC522_NTAG424::ntag424_encrypt(uint8_t *key, uint8_t *iv,
                                         uint8_t length, uint8_t *input,
                                         uint8_t *output) {
  return NTAG424_Handler::ntag424_encrypt(key, iv, length, input, output);
}

uint8_t MFRC522_NTAG424::ntag424_decrypt(uint8_t *key, uint8_t length,
                                         uint8_t *input, uint8_t *output) {
  return NTAG424_Handler::ntag424_decrypt(key, length, input, output);
}

uint8_t MFRC522_NTAG424::ntag424_decrypt(uint8_t *key, uint8_t *iv,
                                         uint8_t length, uint8_t *input,
                                         uint8_t *output) {
  return NTAG424_Handler::ntag424_decrypt(key, iv, length, input, output);
}

uint8_t MFRC522_NTAG424::ntag424_cmac_short(uint8_t *key, uint8_t *input,
                                             uint8_t length, uint8_t *cmac) {
  return NTAG424_Handler::ntag424_cmac_short(key, input, length, cmac);
}

uint8_t MFRC522_NTAG424::ntag424_cmac(uint8_t *key, uint8_t *input,
                                       uint8_t length, uint8_t *cmac) {
  return NTAG424_Handler::ntag424_cmac(key, input, length, cmac);
}

uint8_t MFRC522_NTAG424::ntag424_MAC(uint8_t *cmd, uint8_t *cmdheader,
                                      uint8_t cmdheader_length,
                                      uint8_t *cmddata,
                                      uint8_t cmddata_length,
                                      uint8_t *signature) {
  return NTAG424_Handler::ntag424_MAC(cmd, cmdheader, cmdheader_length,
                                      cmddata, cmddata_length, signature);
}

uint8_t MFRC522_NTAG424::ntag424_MAC(uint8_t *key, uint8_t *cmd,
                                      uint8_t *cmdheader,
                                      uint8_t cmdheader_length,
                                      uint8_t *cmddata,
                                      uint8_t cmddata_length,
                                      uint8_t *signature) {
  return NTAG424_Handler::ntag424_MAC(key, cmd, cmdheader, cmdheader_length,
                                      cmddata, cmddata_length, signature);
}

void MFRC522_NTAG424::ntag424_random(uint8_t *output, uint8_t bytecount) {
  NTAG424_Handler::ntag424_random(output, bytecount);
}

void MFRC522_NTAG424::ntag424_derive_session_keys(uint8_t *key, uint8_t *RndA,
                                                   uint8_t *RndB) {
  NTAG424_Handler::ntag424_derive_session_keys(key, RndA, RndB);
}

uint8_t MFRC522_NTAG424::ntag424_rotl(uint8_t *input, uint8_t *output,
                                       uint8_t bufferlen, uint8_t rotation) {
  return NTAG424_Handler::ntag424_rotl(input, output, bufferlen, rotation);
}

uint8_t MFRC522_NTAG424::ntag424_ReadData(uint8_t *buffer, int fileno,
                                          int offset, int size) {
  return NTAG424_Handler::ntag424_ReadData(buffer, fileno, offset, size);
}

bool MFRC522_NTAG424::ntag424_WriteData(uint8_t fileno, uint8_t *data,
                                        uint8_t length, int offset) {
  return NTAG424_Handler::ntag424_WriteData(fileno, data, length, offset);
}

uint8_t MFRC522_NTAG424::ntag424_Authenticate(uint8_t *key, uint8_t keyno,
                                                 uint8_t cmd) {
  return NTAG424_Handler::ntag424_Authenticate(key, keyno, cmd);
}

uint8_t MFRC522_NTAG424::ntag424_ISOAuthenticate(uint8_t *key,
                                                 uint8_t keyno) {
  return NTAG424_Handler::ntag424_ISOAuthenticate(key, keyno);
}

uint8_t MFRC522_NTAG424::ntag424_ChangeKey(uint8_t *oldkey, uint8_t *newkey,
                                            uint8_t keynumber,
                                            uint8_t keyversion) {
  return NTAG424_Handler::ntag424_ChangeKey(oldkey, newkey, keynumber,
                                            keyversion);
}

uint8_t MFRC522_NTAG424::ntag424_ReadSig(uint8_t *buffer) {
  return NTAG424_Handler::ntag424_ReadSig(buffer);
}

uint8_t MFRC522_NTAG424::ntag424_GetTTStatus(uint8_t *buffer) {
  return NTAG424_Handler::ntag424_GetTTStatus(buffer);
}

uint8_t MFRC522_NTAG424::ntag424_GetCardUID(uint8_t *buffer) {
  return NTAG424_Handler::ntag424_GetCardUID(buffer);
}

bool MFRC522_NTAG424::ntag424_GetKeyVersion(uint8_t keyno, uint8_t *version) {
  return NTAG424_Handler::ntag424_GetKeyVersion(keyno, version);
}

uint8_t MFRC522_NTAG424::ntag424_GetFileSettings(uint8_t fileno,
                                                  uint8_t *buffer,
                                                  uint8_t comm_mode) {
  return NTAG424_Handler::ntag424_GetFileSettings(fileno, buffer, comm_mode);
}

uint8_t MFRC522_NTAG424::ntag424_ChangeFileSettings(
    uint8_t fileno, uint8_t *filesettings, uint8_t filesettings_length,
    uint8_t comm_mode) {
  return NTAG424_Handler::ntag424_ChangeFileSettings(
      fileno, filesettings, filesettings_length, comm_mode);
}

bool MFRC522_NTAG424::ntag424_ISOSelectNDEFFile() {
  return NTAG424_Handler::ntag424_ISOSelectNDEFFile();
}

bool MFRC522_NTAG424::ntag424_ISOSelectCCFile() {
  return NTAG424_Handler::ntag424_ISOSelectCCFile();
}

int16_t MFRC522_NTAG424::ntag424_ReadNDEFMessage(uint8_t *buffer,
                                                  uint16_t maxsize) {
  return NTAG424_Handler::ntag424_ReadNDEFMessage(buffer, maxsize);
}

uint8_t MFRC522_NTAG424::ntag424_ISOReadFile(uint8_t *buffer, int maxsize) {
  return NTAG424_Handler::ntag424_ISOReadFile(buffer, maxsize);
}

uint8_t MFRC522_NTAG424::ntag424_ISOReadBinary(uint16_t offset, uint8_t le,
                                                uint8_t *response,
                                                uint16_t response_bufsize) {
  return NTAG424_Handler::ntag424_ISOReadBinary(offset, le, response,
                                                response_bufsize);
}

bool MFRC522_NTAG424::ntag424_FormatNDEF() {
  return NTAG424_Handler::ntag424_FormatNDEF();
}

bool MFRC522_NTAG424::ntag424_ISOUpdateBinary(uint8_t *buffer,
                                              uint8_t length) {
  return NTAG424_Handler::ntag424_ISOUpdateBinary(buffer, length);
}

bool MFRC522_NTAG424::ntag424_ISOSelectFileById(int fileid) {
  return NTAG424_Handler::ntag424_ISOSelectFileById(fileid);
}

bool MFRC522_NTAG424::ntag424_ISOSelectFileByDFN(uint8_t *dfn) {
  return NTAG424_Handler::ntag424_ISOSelectFileByDFN(dfn);
}

uint8_t MFRC522_NTAG424::ntag424_isNTAG424() {
  return NTAG424_Handler::ntag424_isNTAG424();
}

uint8_t MFRC522_NTAG424::ntag424_GetVersion() {
  return NTAG424_Handler::ntag424_GetVersion();
}
