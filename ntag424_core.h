#ifndef NTAG424_CORE_H
#define NTAG424_CORE_H

#include "ntag424_crypto.h"
#include "ntag424_apdu.h"
#include "ntag424_reader.h"

#define NTAG424_COMM_MODE_PLAIN (0x00)
#define NTAG424_COMM_MODE_MAC (0x01)
#define NTAG424_COMM_MODE_FULL (0x02)
#define NTAG424_COM_CLA (0x90)
#define NTAG424_COM_CHANGEKEY (0xC4)
#define NTAG424_CMD_READSIG (0x3C)
#define NTAG424_CMD_GETTTSTATUS (0xF7)
#define NTAG424_CMD_GETFILESETTINGS (0xF5)
#define NTAG424_CMD_CHANGEFILESETTINGS (0x5F)
#define NTAG424_CMD_GETCARDUUID (0x51)
#define NTAG424_CMD_READDATA (0xAD)
#define NTAG424_CMD_WRITEDATA (0x8D)
#define NTAG424_CMD_GETVERSION (0x60)
#define NTAG424_CMD_NEXTFRAME (0xAF)

#define NTAG424_RESPONE_GETVERSION_HWTYPE_NTAG424 (0x04)

#define NTAG424_COM_ISOCLA (0x00)
#define NTAG424_CMD_ISOSELECTFILE (0xA4)
#define NTAG424_CMD_ISOREADBINARY (0xB0)
#define NTAG424_CMD_ISOUPDATEBINARY (0xD6)

#define NTAG424_AUTHRESPONSE_ENC_SIZE 32
#define NTAG424_AUTHRESPONSE_RNDA_SIZE 16
#define NTAG424_AUTHRESPONSE_PDCAP2_SIZE 6
#define NTAG424_AUTHRESPONSE_PCDCAP2_SIZE 6

#define NTAG424_AUTHRESPONSE_TI_OFFSET 0
#define NTAG424_AUTHRESPONSE_RNDA_OFFSET NTAG424_AUTHRESPONSE_TI_SIZE
#define NTAG424_AUTHRESPONSE_PDCAP2_OFFSET                                   \
  NTAG424_AUTHRESPONSE_TI_SIZE + NTAG424_AUTHRESPONSE_RNDA_SIZE
#define NTAG424_AUTHRESPONSE_PCDCAP2_OFFSET                                  \
  NTAG424_AUTHRESPONSE_TI_SIZE + NTAG424_AUTHRESPONSE_RNDA_SIZE +           \
      NTAG424_AUTHRESPONSE_PDCAP2_SIZE

struct ntag424_VersionInfoType {
  uint8_t VendorID;
  uint8_t HWType;
  uint8_t HWSubType;
  uint8_t HWMajorVersion;
  uint8_t HWMinorVersion;
  uint8_t HWStorageSize;
  uint8_t HWProtocol;
  uint8_t SWType;
  uint8_t SWSubType;
  uint8_t SWMajorVersion;
  uint8_t SWMinorVersion;
  uint8_t SWStorageSize;
  uint8_t SWProtocol;
  uint8_t UID[7];
  uint8_t BatchNo[5];
  uint8_t FabKey[2];
  uint8_t CWProd;
  uint8_t YearProd;
  uint8_t FabKeyID;
};

struct ntag424_FileSettings {
  uint8_t FileType;
  uint8_t FileOption;
  uint8_t AccessRights;
  uint8_t FileSize;
  uint8_t SDMOptions;
  uint8_t SMDAccessRights;
  uint8_t UIDOffset;
  uint8_t SDMReadCtrOffset;
  uint8_t PICCDataOffset;
  uint8_t TTStatusOffset;
  uint8_t SDMMACInputOffset;
  uint8_t SDMENCOffset;
  uint8_t SDMENCLength;
  uint8_t SDMMACOffset;
  uint8_t SDMReadCtrlLimit;
};

uint8_t ntag424_Authenticate(NTAG424_Reader *reader,
                             ntag424_SessionType *session, uint8_t *key,
                             uint8_t keyno, uint8_t cmd);
uint8_t ntag424_ISOAuthenticate(NTAG424_Reader *reader,
                                ntag424_SessionType *session, uint8_t *key,
                                uint8_t keyno);
uint8_t ntag424_GetFileSettings(NTAG424_Reader *reader,
                                 ntag424_SessionType *session,
                                 uint8_t fileno, uint8_t *buffer,
                                 uint8_t comm_mode);
uint8_t ntag424_ChangeFileSettings(NTAG424_Reader *reader,
                                    ntag424_SessionType *session,
                                    uint8_t fileno, uint8_t *filesettings,
                                    uint8_t filesettings_length,
                                    uint8_t comm_mode);
uint8_t ntag424_ChangeKey(NTAG424_Reader *reader, ntag424_SessionType *session,
                          uint8_t *oldkey, uint8_t *newkey, uint8_t keynumber,
                          uint8_t keyversion = 0x01);
uint8_t ntag424_GetCardUID(NTAG424_Reader *reader,
                           ntag424_SessionType *session, uint8_t *buffer);
bool ntag424_GetKeyVersion(NTAG424_Reader *reader, ntag424_SessionType *session,
                           uint8_t keyno, uint8_t *version);
uint8_t ntag424_GetTTStatus(NTAG424_Reader *reader,
                            ntag424_SessionType *session, uint8_t *buffer);
uint8_t ntag424_ReadSig(NTAG424_Reader *reader, ntag424_SessionType *session,
                        uint8_t *buffer);
uint8_t ntag424_ReadData(NTAG424_Reader *reader, uint8_t *buffer, int fileno,
                         int offset, int size);
bool ntag424_WriteData(NTAG424_Reader *reader, ntag424_SessionType *session,
                       uint8_t fileno, uint8_t *data, uint8_t length);
uint8_t ntag424_isNTAG424(NTAG424_Reader *reader,
                          ntag424_SessionType *session,
                          ntag424_VersionInfoType *version_info);
uint8_t ntag424_GetVersion(NTAG424_Reader *reader,
                           ntag424_VersionInfoType *version_info);
bool ntag424_FormatNDEF(NTAG424_Reader *reader);
bool ntag424_ISOUpdateBinary(NTAG424_Reader *reader, uint8_t *data_to_write,
                             uint8_t length);
bool ntag424_ISOSelectFileById(NTAG424_Reader *reader,
                               ntag424_SessionType *session, int fileid);
bool ntag424_ISOSelectFileByDFN(NTAG424_Reader *reader,
                                ntag424_SessionType *session, uint8_t *dfn);
bool ntag424_ISOSelectCCFile(NTAG424_Reader *reader,
                             ntag424_SessionType *session);
bool ntag424_ISOSelectNDEFFile(NTAG424_Reader *reader,
                               ntag424_SessionType *session);
int16_t ntag424_ReadNDEFMessage(NTAG424_Reader *reader, uint8_t *buffer,
                                uint16_t maxsize);
uint8_t ntag424_ISOReadFile(NTAG424_Reader *reader, uint8_t *buffer,
                            int maxsize);
uint8_t ntag424_ISOReadBinary(NTAG424_Reader *reader, uint16_t offset,
                              uint8_t le, uint8_t *response,
                              uint16_t response_bufsize);

#endif
