#ifndef NTAG424_CORE_H
#define NTAG424_CORE_H

#include "ntag424_crypto.h"
#include "ntag424_apdu.h"
#include "ntag424_reader.h"

// ============================================================================
// Communication mode flags — NTAG424 DNA Application Note AN12196 §4.1
// Ref: https://www.nxp.com/docs/en/application-note/AN12196.pdf
// PLAIN = no cryptographic protection on command/response
// MAC   = CMAC appended to command, verified on response (§4.1.2)
// FULL  = AES-128 encryption + CMAC (§4.1.3)
// ============================================================================
#define NTAG424_COMM_MODE_PLAIN (0x00)
#define NTAG424_COMM_MODE_MAC (0x01)
#define NTAG424_COMM_MODE_FULL (0x02)

// ============================================================================
// CLA byte values — NTAG424 DNA datasheet (NT4H2421Gx) §6.3.2
// Ref: https://www.nxp.com/docs/en/data-sheet/NT4H2421Gx.pdf
//
// The NTAG424 DNA uses ISO/IEC 7816-4 "wrapped native" command format.
// All proprietary commands use CLA=0x90. Standard ISO commands use CLA=0x00.
// "Communication via native ISO/IEC7816-4 commands without wrapping is not
//  supported." — NT4H2421Gx datasheet §6.3.2
//
// Confirmed by reference implementation:
//   Obsttube/MFRC522_NTAG424DNA — sendData[0] = 0x90 for all native cmds
//   https://github.com/Obsttube/MFRC522_NTAG424DNA
// ============================================================================
#define NTAG424_COM_CLA (0x90)    // Wrapped native command CLA
#define NTAG424_COM_ISOCLA (0x00) // Standard ISO 7816-4 CLA

// ============================================================================
// Native command INS codes — NTAG424 DNA datasheet §7.x
// Each maps to a proprietary NTAG424 command wrapped in ISO 7816-4 format.
// ============================================================================
#define NTAG424_COM_CHANGEKEY (0xC4)           // §7.3.2 ChangeKey
#define NTAG424_CMD_READSIG (0x3C)             // §7.5.3 ReadSign
#define NTAG424_CMD_GETTTSTATUS (0xF7)         // §7.6.4 GetTTStatus
#define NTAG424_CMD_GETFILESETTINGS (0xF5)     // §7.6.3 GetFileSettings
#define NTAG424_CMD_CHANGEFILESETTINGS (0x5F)  // §7.6.2 ChangeFileSettings
#define NTAG424_CMD_GETCARDUUID (0x51)         // §7.5.1 GetCardUID
#define NTAG424_CMD_READDATA (0xAD)            // §7.6.6 ReadData
#define NTAG424_CMD_WRITEDATA (0x8D)           // §7.6.5 WriteData
#define NTAG424_CMD_GETVERSION (0x60)          // §7.2.1 GetVersion
#define NTAG424_CMD_NEXTFRAME (0xAF)           // §7.1.2 Additional Frame (chaining)

// ============================================================================
// ISO 7816-4 command INS codes — standard commands supported by NTAG424 DNA
// Ref: ISO/IEC 7816-4, NT4H2421Gx datasheet §6.4
// ============================================================================
#define NTAG424_CMD_ISOSELECTFILE (0xA4)    // ISO 7816-4 SELECT
#define NTAG424_CMD_ISOREADBINARY (0xB0)    // ISO 7816-4 READ BINARY
#define NTAG424_CMD_ISOUPDATEBINARY (0xD6)  // ISO 7816-4 UPDATE BINARY

// ============================================================================
// GetVersion response HW type — NTAG424 DNA datasheet §7.2.1
// HWType=0x04 identifies the tag as NTAG424 DNA family.
// ============================================================================
#define NTAG424_RESPONE_GETVERSION_HWTYPE_NTAG424 (0x04)

// ============================================================================
// Authentication delay handling — AN12196 §6.4, NT4H2421Gx datasheet §9.5.3
//
// Status 91 AD = AUTHENTICATION_DELAY. The card refuses to process auth
// because its SeqFailCtr ≥ 50 (too many consecutive failed attempts).
// The card has no RTC — the "delay" is measured in FWT multiples stored
// in non-volatile EEPROM counters (SpentTimeCtr). Keep retrying until
// the delay is spent.
//
// Confirmed by Flipper Zero nxp_native_command.h:
//   "Currently not allowed to authenticate. Keep trying until full delay is spent"
//
// MAX_RETRIES=50 with 50ms spacing gives ~5s total budget. Each 91 AD
// response takes ~25ms (65% of FWT=38.66ms). This is enough to ride out
// most SpentTimeCtr accumulations without risking TotFailCtr overflow
// (TotFailCtr limit = 50, per datasheet §9.5.3).
// ============================================================================
// ============================================================================
// NTAG424 DNA Status Codes (SW1=0x91, SW2 varies) — NT4H2421Gx datasheet Table 62
// Ref: https://www.nxp.com/docs/en/data-sheet/NT4H2421Gx.pdf
//
// All native NTAG424 commands return a 2-byte status at the end of the response.
// SW1=0x90 indicates success in ISO 7816-4 native mode (status byte 0x00).
// SW1=0x91 indicates a proprietary NTAG424 status in SW2.
//
// Confirmed by reference implementations:
//   Obsttube/MFRC522_NTAG424DNA — checks for 0x91 status codes
//   Flipper Zero nxp_native_command.h — NTAG424 status code definitions
// ============================================================================
#define NTAG424_STATUS_SUCCESS_SW1       (0x90)  // ISO success (native mode, SW2=0x00)
#define NTAG424_STATUS_SUCCESS_SW2       (0x00)  // Operation completed successfully
#define NTAG424_STATUS_INTEGRITY_SW2     (0x1E)  // INTEGRITY_ERROR: CMAC verification failed
                                                 // — secure messaging MAC mismatch between
                                                 //   PCD and PICC. Per §9.1.8, caused by
                                                 //   cmd_counter desync, wrong session key,
                                                 //   or tampered data.
#define NTAG424_STATUS_NO_SUCH_KEY_SW2   (0x06)  // NO_SUCH_KEY: Requested key number does not exist
#define NTAG424_STATUS_LENGTH_SW2        (0x7E)  // LENGTH_ERROR: Command data length not allowed
                                                 // — APDU Lc value exceeds file capacity or
                                                 //   protocol limit.
#define NTAG424_STATUS_AUTH_DELAY_SW2    (0xAD)  // AUTHENTICATION_DELAY: Card is throttling due to
                                                 // failed auth attempts. Keep retrying until the
                                                 // EEPROM-based delay counter (SpentTimeCtr) expires.
                                                 // Per §9.5.3: SeqFailCtr ≥ 50 triggers delay.
#define NTAG424_STATUS_AUTH_ERROR_SW2    (0xAE)  // AUTHENTICATION_ERROR: No valid authentication
                                                 // session exists for the requested operation.
                                                 // Either auth was never performed, the session
                                                 // was invalidated, or a prior MAC error
                                                 // corrupted the secure messaging state.
#define NTAG424_STATUS_ADDITIONAL_FRAME  (0xAF)  // Additional Frame (SW1=0x90): command chaining
                                                 // — more data expected. NOT an error.
                                                 // Already defined as NTAG424_CMD_NEXTFRAME (0xAF).
#define NTAG424_STATUS_NOT_ALLOWED_SW2   (0x9D)  // COMMAND_NOT_ALLOWED: Current state does not
                                                 // permit this command (e.g. wrong access rights).
#define NTAG424_STATUS_EOF_SW2           (0xBE)  // BOUNDARY_ERROR: Attempted to read/write past
                                                 // the end of a file.

// ============================================================================
// Authentication delay handling — AN12196 §6.4, NT4H2421Gx datasheet §9.5.3
//
// MAX_RETRIES=50 with ~50ms spacing gives ~2.5s total budget. Each 91 AD
// response takes ~25ms (65% of FWT=38.66ms). This is enough to ride out
// most SpentTimeCtr accumulations without risking TotFailCtr overflow
// (TotFailCtr limit = 50, per datasheet §9.5.3).
// ============================================================================
#define NTAG424_AUTH_DELAY_MAX_RETRIES 50

// ============================================================================
// Authentication response structure — NT4H2421Gx datasheet §7.3.1.3
// The AuthenticateEV2 Part2 response (after decryption) is 32 bytes:
//   [0..3]   TI (4 bytes) — Transaction Identifier
//   [4..19]  RndA' (16 bytes) — Rotated copy of PCD random, proves card
//   [20..25] PDCAP2 (6 bytes) — PICC capability information
//   [26..31] PCDCAP2 (6 bytes) — PCD capability information
// Ref: AN12196 §5.2, NT4H2421Gx datasheet Table 36
// ============================================================================
#define NTAG424_AUTHRESPONSE_ENC_SIZE 32   // Total encrypted response: 32 bytes
#define NTAG424_AUTHRESPONSE_RNDA_SIZE 16  // AES-128 block size: 16 bytes
#define NTAG424_AUTHRESPONSE_PDCAP2_SIZE 6  // PICC capabilities: 6 bytes
#define NTAG424_AUTHRESPONSE_PCDCAP2_SIZE 6  // PCD capabilities: 6 bytes

// ============================================================================
// Authentication response field offsets — derived from the structure above
// ============================================================================
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
                        uint8_t fileno, uint8_t *data, uint8_t length,
                        int offset = 0);
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
