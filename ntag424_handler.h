#ifndef NTAG424_HANDLER_H
#define NTAG424_HANDLER_H

#include "ntag424_core.h"

class NTAG424_Handler {
public:
  explicit NTAG424_Handler(NTAG424_Reader *reader = nullptr);

  void attach_reader(NTAG424_Reader *reader);

  uint8_t ntag424_apdu_send(uint8_t *cla, uint8_t *ins, uint8_t *p1,
                            uint8_t *p2, uint8_t *cmd_header,
                            uint8_t cmd_header_length, uint8_t *cmd_data,
                            uint8_t cmd_data_length, uint8_t le,
                            uint8_t comm_mode, uint8_t *response,
                            uint8_t response_le);
  uint32_t ntag424_crc32(uint8_t *data, uint8_t datalength);
  uint8_t ntag424_addpadding(uint8_t inputlength, uint8_t paddinglength,
                             uint8_t *buffer);
  uint8_t ntag424_encrypt(uint8_t *key, uint8_t length, uint8_t *input,
                          uint8_t *output);
  uint8_t ntag424_encrypt(uint8_t *key, uint8_t *iv, uint8_t length,
                          uint8_t *input, uint8_t *output);
  uint8_t ntag424_decrypt(uint8_t *key, uint8_t length, uint8_t *input,
                          uint8_t *output);
  uint8_t ntag424_decrypt(uint8_t *key, uint8_t *iv, uint8_t length,
                          uint8_t *input, uint8_t *output);
  uint8_t ntag424_cmac_short(uint8_t *key, uint8_t *input, uint8_t length,
                             uint8_t *cmac);
  uint8_t ntag424_cmac(uint8_t *key, uint8_t *input, uint8_t length,
                       uint8_t *cmac);
  uint8_t ntag424_MAC(uint8_t *cmd, uint8_t *cmdheader,
                      uint8_t cmdheader_length, uint8_t *cmddata,
                      uint8_t cmddata_length, uint8_t *signature);
  uint8_t ntag424_MAC(uint8_t *key, uint8_t *cmd, uint8_t *cmdheader,
                      uint8_t cmdheader_length, uint8_t *cmddata,
                      uint8_t cmddata_length, uint8_t *signature);
  void ntag424_random(uint8_t *output, uint8_t bytecount);
  void ntag424_derive_session_keys(uint8_t *key, uint8_t *RndA, uint8_t *RndB);
  uint8_t ntag424_rotl(uint8_t *input, uint8_t *output, uint8_t bufferlen,
                       uint8_t rotation);
  uint8_t ntag424_ReadData(uint8_t *buffer, int fileno, int offset, int size);
  uint8_t ntag424_Authenticate(uint8_t *key, uint8_t keyno, uint8_t cmd);
  uint8_t ntag424_ChangeKey(uint8_t *oldkey, uint8_t *newkey,
                            uint8_t keynumber, uint8_t keyversion = 0x01);
  uint8_t ntag424_ReadSig(uint8_t *buffer);
  uint8_t ntag424_GetTTStatus(uint8_t *buffer);
  uint8_t ntag424_GetCardUID(uint8_t *buffer);
  bool ntag424_GetKeyVersion(uint8_t keyno, uint8_t *version);
  uint8_t ntag424_GetFileSettings(uint8_t fileno, uint8_t *buffer,
                                  uint8_t comm_mode);
  uint8_t ntag424_ChangeFileSettings(uint8_t fileno, uint8_t *filesettings,
                                     uint8_t filesettings_length,
                                     uint8_t comm_mode);
  bool ntag424_ISOSelectNDEFFile();
  bool ntag424_ISOSelectCCFile();
  int16_t ntag424_ReadNDEFMessage(uint8_t *buffer, uint16_t maxsize);
  uint8_t ntag424_ISOReadFile(uint8_t *buffer, int maxsize);
  uint8_t ntag424_ISOReadBinary(uint16_t offset, uint8_t le, uint8_t *response,
                                uint16_t response_bufsize);
  bool ntag424_FormatNDEF();
  bool ntag424_ISOUpdateBinary(uint8_t *buffer, uint8_t length);
  bool ntag424_ISOSelectFileById(int fileid);
  bool ntag424_ISOSelectFileByDFN(uint8_t *dfn);
  uint8_t ntag424_isNTAG424();
  uint8_t ntag424_GetVersion();

  ntag424_SessionType ntag424_Session = {};
  ntag424_VersionInfoType ntag424_VersionInfo = {};
  uint8_t (&ntag424_authresponse_TI)[NTAG424_AUTHRESPONSE_TI_SIZE];
  uint8_t (&ntag424_authresponse_RNDA)[NTAG424_AUTHRESPONSE_RNDA_SIZE];
  uint8_t (&ntag424_authresponse_PDCAP2)[NTAG424_AUTHRESPONSE_PDCAP2_SIZE];
  uint8_t (&ntag424_authresponse_PCDCAP2)[NTAG424_AUTHRESPONSE_PCDCAP2_SIZE];

protected:
  NTAG424_Reader *_ntag424_reader = nullptr;
};

#endif
