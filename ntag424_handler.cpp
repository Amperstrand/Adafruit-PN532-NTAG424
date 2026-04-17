#include "ntag424_handler.h"

#include <string.h>

namespace {

constexpr uint8_t kNtag424MaxApduSize = 80;

}

NTAG424_Handler::NTAG424_Handler(NTAG424_Reader *reader)
    : ntag424_authresponse_TI(ntag424_Session.TI),
      ntag424_authresponse_RNDA(ntag424_Session.RndA),
      ntag424_authresponse_PDCAP2(ntag424_Session.PDCAP2),
      ntag424_authresponse_PCDCAP2(ntag424_Session.PCDCAP2),
      _ntag424_reader(reader) {}

void NTAG424_Handler::attach_reader(NTAG424_Reader *reader) {
  _ntag424_reader = reader;
}

uint8_t NTAG424_Handler::ntag424_apdu_send(
    uint8_t *cla, uint8_t *ins, uint8_t *p1, uint8_t *p2, uint8_t *cmd_header,
    uint8_t cmd_header_length, uint8_t *cmd_data, uint8_t cmd_data_length,
    uint8_t le, uint8_t comm_mode, uint8_t *response, uint8_t response_le) {
  if (_ntag424_reader == nullptr || cla == nullptr || ins == nullptr ||
      p1 == nullptr || p2 == nullptr || response == nullptr || response_le == 0) {
    return 0;
  }

  uint8_t apdu[kNtag424MaxApduSize] = {0};
  const uint8_t apdu_length = ::ntag424_build_apdu(
      cla[0], ins[0], p1[0], p2[0], cmd_header, cmd_header_length, cmd_data,
      cmd_data_length, le, comm_mode, &ntag424_Session, apdu);
  if (apdu_length == 0) {
    return 0;
  }

  uint8_t raw_response[kNtag424MaxApduSize] = {0};
  const uint8_t raw_response_length = _ntag424_reader->transceive(
      apdu, apdu_length, raw_response, sizeof(raw_response));
  if (raw_response_length == 0) {
    return 0;
  }

  uint8_t processed_response[kNtag424MaxApduSize] = {0};
  const uint8_t processed_response_length = ::ntag424_process_response(
      raw_response, raw_response_length, comm_mode, &ntag424_Session,
      processed_response);
  if (processed_response_length == 0 || processed_response_length > response_le) {
    return 0;
  }

  memcpy(response, processed_response, processed_response_length);
  return processed_response_length;
}

uint32_t NTAG424_Handler::ntag424_crc32(uint8_t *data, uint8_t datalength) {
  return ::ntag424_crc32(data, datalength);
}

uint8_t NTAG424_Handler::ntag424_addpadding(uint8_t inputlength,
                                            uint8_t paddinglength,
                                            uint8_t *buffer) {
  return ::ntag424_addpadding(inputlength, paddinglength, buffer);
}

uint8_t NTAG424_Handler::ntag424_encrypt(uint8_t *key, uint8_t length,
                                         uint8_t *input, uint8_t *output) {
  return ::ntag424_encrypt(key, length, input, output);
}

uint8_t NTAG424_Handler::ntag424_encrypt(uint8_t *key, uint8_t *iv,
                                         uint8_t length, uint8_t *input,
                                         uint8_t *output) {
  return ::ntag424_encrypt(key, iv, length, input, output);
}

uint8_t NTAG424_Handler::ntag424_decrypt(uint8_t *key, uint8_t length,
                                         uint8_t *input, uint8_t *output) {
  return ::ntag424_decrypt(key, length, input, output);
}

uint8_t NTAG424_Handler::ntag424_decrypt(uint8_t *key, uint8_t *iv,
                                         uint8_t length, uint8_t *input,
                                         uint8_t *output) {
  return ::ntag424_decrypt(key, iv, length, input, output);
}

uint8_t NTAG424_Handler::ntag424_cmac_short(uint8_t *key, uint8_t *input,
                                            uint8_t length, uint8_t *cmac) {
  return ::ntag424_cmac_short(key, input, length, cmac);
}

uint8_t NTAG424_Handler::ntag424_cmac(uint8_t *key, uint8_t *input,
                                      uint8_t length, uint8_t *cmac) {
  return ::ntag424_cmac(key, input, length, cmac);
}

uint8_t NTAG424_Handler::ntag424_MAC(uint8_t *cmd, uint8_t *cmdheader,
                                     uint8_t cmdheader_length,
                                     uint8_t *cmddata,
                                     uint8_t cmddata_length,
                                     uint8_t *signature) {
  return ::ntag424_MAC(&ntag424_Session, cmd, cmdheader, cmdheader_length,
                       cmddata, cmddata_length, signature);
}

uint8_t NTAG424_Handler::ntag424_MAC(uint8_t *key, uint8_t *cmd,
                                     uint8_t *cmdheader,
                                     uint8_t cmdheader_length,
                                     uint8_t *cmddata,
                                     uint8_t cmddata_length,
                                     uint8_t *signature) {
  return ::ntag424_MAC(&ntag424_Session, key, cmd, cmdheader,
                       cmdheader_length, cmddata, cmddata_length, signature);
}

void NTAG424_Handler::ntag424_random(uint8_t *output, uint8_t bytecount) {
  ::ntag424_random(output, bytecount);
}

void NTAG424_Handler::ntag424_derive_session_keys(uint8_t *key, uint8_t *RndA,
                                                  uint8_t *RndB) {
  ::ntag424_derive_session_keys(&ntag424_Session, key, RndA, RndB);
}

uint8_t NTAG424_Handler::ntag424_rotl(uint8_t *input, uint8_t *output,
                                      uint8_t bufferlen, uint8_t rotation) {
  return ::ntag424_rotl(input, output, bufferlen, rotation);
}

uint8_t NTAG424_Handler::ntag424_ReadData(uint8_t *buffer, int fileno,
                                          int offset, int size) {
  return ::ntag424_ReadData(_ntag424_reader, buffer, fileno, offset, size);
}

bool NTAG424_Handler::ntag424_WriteData(uint8_t fileno, uint8_t *data,
                                        uint8_t length) {
  return ::ntag424_WriteData(_ntag424_reader, &ntag424_Session, fileno, data,
                             length);
}

uint8_t NTAG424_Handler::ntag424_Authenticate(uint8_t *key, uint8_t keyno,
                                              uint8_t cmd) {
  return ::ntag424_Authenticate(_ntag424_reader, &ntag424_Session, key, keyno,
                                cmd);
}

uint8_t NTAG424_Handler::ntag424_ChangeKey(uint8_t *oldkey, uint8_t *newkey,
                                           uint8_t keynumber,
                                           uint8_t keyversion) {
  return ::ntag424_ChangeKey(_ntag424_reader, &ntag424_Session, oldkey, newkey,
                             keynumber, keyversion);
}

uint8_t NTAG424_Handler::ntag424_ReadSig(uint8_t *buffer) {
  return ::ntag424_ReadSig(_ntag424_reader, &ntag424_Session, buffer);
}

uint8_t NTAG424_Handler::ntag424_GetTTStatus(uint8_t *buffer) {
  return ::ntag424_GetTTStatus(_ntag424_reader, &ntag424_Session, buffer);
}

uint8_t NTAG424_Handler::ntag424_GetCardUID(uint8_t *buffer) {
  return ::ntag424_GetCardUID(_ntag424_reader, &ntag424_Session, buffer);
}

bool NTAG424_Handler::ntag424_GetKeyVersion(uint8_t keyno, uint8_t *version) {
  return ::ntag424_GetKeyVersion(_ntag424_reader, &ntag424_Session, keyno,
                                 version);
}

uint8_t NTAG424_Handler::ntag424_GetFileSettings(uint8_t fileno,
                                                 uint8_t *buffer,
                                                 uint8_t comm_mode) {
  return ::ntag424_GetFileSettings(_ntag424_reader, &ntag424_Session, fileno,
                                   buffer, comm_mode);
}

uint8_t NTAG424_Handler::ntag424_ChangeFileSettings(
    uint8_t fileno, uint8_t *filesettings, uint8_t filesettings_length,
    uint8_t comm_mode) {
  return ::ntag424_ChangeFileSettings(_ntag424_reader, &ntag424_Session,
                                      fileno, filesettings,
                                      filesettings_length, comm_mode);
}

bool NTAG424_Handler::ntag424_ISOSelectNDEFFile() {
  return ::ntag424_ISOSelectNDEFFile(_ntag424_reader, &ntag424_Session);
}

bool NTAG424_Handler::ntag424_ISOSelectCCFile() {
  return ::ntag424_ISOSelectCCFile(_ntag424_reader, &ntag424_Session);
}

int16_t NTAG424_Handler::ntag424_ReadNDEFMessage(uint8_t *buffer,
                                                 uint16_t maxsize) {
  return ::ntag424_ReadNDEFMessage(_ntag424_reader, buffer, maxsize);
}

uint8_t NTAG424_Handler::ntag424_ISOReadFile(uint8_t *buffer, int maxsize) {
  return ::ntag424_ISOReadFile(_ntag424_reader, buffer, maxsize);
}

uint8_t NTAG424_Handler::ntag424_ISOReadBinary(uint16_t offset, uint8_t le,
                                               uint8_t *response,
                                               uint16_t response_bufsize) {
  return ::ntag424_ISOReadBinary(_ntag424_reader, offset, le, response,
                                 response_bufsize);
}

bool NTAG424_Handler::ntag424_FormatNDEF() {
  return ::ntag424_FormatNDEF(_ntag424_reader);
}

bool NTAG424_Handler::ntag424_ISOSelectFileById(int fileid) {
  return ::ntag424_ISOSelectFileById(_ntag424_reader, &ntag424_Session,
                                     fileid);
}

bool NTAG424_Handler::ntag424_ISOSelectFileByDFN(uint8_t *dfn) {
  return ::ntag424_ISOSelectFileByDFN(_ntag424_reader, &ntag424_Session, dfn);
}

uint8_t NTAG424_Handler::ntag424_isNTAG424() {
  return ::ntag424_isNTAG424(_ntag424_reader, &ntag424_Session,
                             &ntag424_VersionInfo);
}

uint8_t NTAG424_Handler::ntag424_GetVersion() {
  return ::ntag424_GetVersion(_ntag424_reader, &ntag424_VersionInfo);
}
