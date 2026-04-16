#include "ntag424_core.h"

#include <string.h>

namespace {

constexpr uint8_t kNtag424MaxApduSize = 80;
constexpr uint8_t kIsoApplicationDfn[7] = {0xD2, 0x76, 0x00, 0x00,
                                           0x85, 0x01, 0x01};

uint8_t ntag424_send_apdu(NTAG424_Reader *reader, ntag424_SessionType *session,
                          const uint8_t *TI, uint8_t cla, uint8_t ins,
                          uint8_t p1, uint8_t p2, const uint8_t *cmd_header,
                          uint8_t cmd_header_length, const uint8_t *cmd_data,
                          uint8_t cmd_data_length, uint8_t le,
                          uint8_t comm_mode, uint8_t *processed,
                          uint8_t processed_size) {
  if (reader == nullptr || processed == nullptr || processed_size == 0) {
    return 0;
  }

  uint8_t apdu[kNtag424MaxApduSize] = {0};
  const uint8_t apdu_length =
      ntag424_build_apdu(cla, ins, p1, p2, cmd_header, cmd_header_length,
                         cmd_data, cmd_data_length, le, comm_mode, session, TI,
                         apdu);
  if (apdu_length == 0) {
    return 0;
  }

  uint8_t response[kNtag424MaxApduSize] = {0};
  const uint8_t response_length =
      reader->transceive(apdu, apdu_length, response, sizeof(response));
  if (response_length == 0) {
    return 0;
  }

  uint8_t decoded[kNtag424MaxApduSize] = {0};
  const uint8_t decoded_length =
      ntag424_process_response(response, response_length, comm_mode, session, TI,
                               decoded);
  if (decoded_length == 0 || decoded_length > processed_size) {
    return 0;
  }

  memcpy(processed, decoded, decoded_length);
  return decoded_length;
}

uint8_t ntag424_build_changekey_payload_local(const uint8_t *oldkey,
                                              const uint8_t *newkey,
                                              uint8_t keynumber,
                                              uint8_t keyversion,
                                              uint32_t jamcrc_newkey,
                                              uint8_t *keydata) {
  if (keynumber == 0) {
    memcpy(keydata, newkey, 16);
    keydata[16] = keyversion;
    return 17;
  }

  for (uint8_t i = 0; i < 16; ++i) {
    keydata[i] = oldkey[i] ^ newkey[i];
  }
  keydata[16] = keyversion;
  memcpy(keydata + 17, &jamcrc_newkey, sizeof(jamcrc_newkey));
  return 21;
}

bool ntag424_status_ok(const uint8_t *response, uint8_t response_length,
                       uint8_t sw1, uint8_t sw2) {
  return response != nullptr && response_length >= 2 &&
         response[response_length - 2] == sw1 &&
         response[response_length - 1] == sw2;
}

uint8_t ntag424_response_data_length(uint8_t response_length) {
  return response_length >= 2 ? static_cast<uint8_t>(response_length - 2) : 0;
}

}

uint8_t ntag424_Authenticate(NTAG424_Reader *reader,
                             ntag424_SessionType *session, uint8_t *TI,
                             uint8_t *key, uint8_t keyno, uint8_t cmd) {
  if (reader == nullptr || session == nullptr || TI == nullptr || key == nullptr) {
    return 0;
  }

  const uint8_t select_file[] = {NTAG424_COM_ISOCLA,
                                 NTAG424_CMD_ISOSELECTFILE,
                                 0x04,
                                 0x00,
                                 0x07,
                                 0xD2,
                                 0x76,
                                 0x00,
                                 0x00,
                                 0x85,
                                 0x01,
                                 0x01,
                                 0x00};
  uint8_t select_response[16] = {0};
  const uint8_t select_length = reader->transceive(
      select_file, sizeof(select_file), select_response, sizeof(select_response));
  if (!ntag424_status_ok(select_response, select_length, 0x90, 0x00)) {
    return 0;
  }

  const uint8_t auth1[] = {NTAG424_COM_CLA, cmd, 0x00, 0x00,
                           0x05,            keyno, 0x03, 0x00,
                           0x00,            0x00, 0x00};
  uint8_t auth1_response[32] = {0};
  const uint8_t auth1_length =
      reader->transceive(auth1, sizeof(auth1), auth1_response,
                         sizeof(auth1_response));
  if (!ntag424_status_ok(auth1_response, auth1_length, 0x91, 0xAF) ||
      ntag424_response_data_length(auth1_length) != 16) {
    return 0;
  }

  const uint8_t blocklength = 16;
  uint8_t RndA[16] = {0};
  uint8_t RndB[16] = {0};
  uint8_t RndBEnc[16] = {0};
  uint8_t RndBRotl[16] = {0};
  uint8_t answer[32] = {0};
  uint8_t answer_enc[32] = {0};
  memcpy(RndBEnc, auth1_response, blocklength);
  if (!ntag424_decrypt(key, blocklength, RndBEnc, RndB)) {
    return 0;
  }

  ntag424_rotl(RndB, RndBRotl, blocklength, 1);
  ntag424_random(RndA, blocklength);
  memcpy(answer, RndA, blocklength);
  memcpy(answer + blocklength, RndBRotl, blocklength);
  ntag424_encrypt(key, sizeof(answer), answer, answer_enc);

  uint8_t auth2[38] = {NTAG424_COM_CLA, NTAG424_CMD_NEXTFRAME, 0x00, 0x00, 0x20};
  memcpy(auth2 + 5, answer_enc, sizeof(answer_enc));
  auth2[37] = 0x00;
  uint8_t auth2_response_enc[NTAG424_AUTHRESPONSE_ENC_SIZE] = {0};
  uint8_t auth2_frame[40] = {0};
  const uint8_t auth2_length =
      reader->transceive(auth2, sizeof(auth2), auth2_frame, sizeof(auth2_frame));
  if (!ntag424_status_ok(auth2_frame, auth2_length, 0x91, 0x00) ||
      ntag424_response_data_length(auth2_length) != NTAG424_AUTHRESPONSE_ENC_SIZE) {
    return 0;
  }

  uint8_t auth2_response[NTAG424_AUTHRESPONSE_ENC_SIZE] = {0};
  memcpy(auth2_response_enc, auth2_frame, sizeof(auth2_response_enc));
  if (!ntag424_decrypt(key, NTAG424_AUTHRESPONSE_ENC_SIZE, auth2_response_enc,
                       auth2_response)) {
    return 0;
  }

  memcpy(TI, auth2_response + NTAG424_AUTHRESPONSE_TI_OFFSET,
         NTAG424_AUTHRESPONSE_TI_SIZE);
  session->cmd_counter = 0;
  session->authenticated = true;
  ntag424_derive_session_keys(session, key, RndA, RndB);
  return 1;
}

uint8_t ntag424_GetFileSettings(NTAG424_Reader *reader,
                                ntag424_SessionType *session,
                                const uint8_t *TI, uint8_t fileno,
                                uint8_t *buffer, uint8_t comm_mode) {
  uint8_t result[64] = {0};
  const uint8_t cmd_header[1] = {fileno};
  const uint8_t result_length =
      ntag424_send_apdu(reader, session, TI, NTAG424_COM_CLA,
                        NTAG424_CMD_GETFILESETTINGS, 0x00, 0x00, cmd_header,
                        sizeof(cmd_header), nullptr, 0, 0, comm_mode, result,
                        sizeof(result));
  if (result_length > 0 && buffer != nullptr) {
    memcpy(buffer, result, result_length);
  }
  return result_length;
}

uint8_t ntag424_ChangeFileSettings(NTAG424_Reader *reader,
                                   ntag424_SessionType *session,
                                   const uint8_t *TI, uint8_t fileno,
                                   uint8_t *filesettings,
                                   uint8_t filesettings_length,
                                   uint8_t comm_mode) {
  uint8_t result[30] = {0};
  const uint8_t cmd_header[1] = {fileno};
  return ntag424_send_apdu(reader, session, TI, NTAG424_COM_CLA,
                           NTAG424_CMD_CHANGEFILESETTINGS, 0x00, 0x00,
                           cmd_header, sizeof(cmd_header), filesettings,
                           filesettings_length, 0, comm_mode, result,
                           sizeof(result));
}

uint8_t ntag424_ChangeKey(NTAG424_Reader *reader, ntag424_SessionType *session,
                          const uint8_t *TI, uint8_t *oldkey,
                          uint8_t *newkey, uint8_t keynumber,
                          uint8_t keyversion) {
  if (oldkey == nullptr || newkey == nullptr) {
    return false;
  }

  uint32_t jamcrc_newkey = ntag424_crc32(newkey, 16);
  jamcrc_newkey = ~jamcrc_newkey;
  uint8_t keydata[32] = {0};
  const uint8_t keydata_length = ntag424_build_changekey_payload_local(
      oldkey, newkey, keynumber, keyversion, jamcrc_newkey, keydata);
  const uint8_t cmd_header[1] = {keynumber};
  uint8_t result[50] = {0};
  const uint8_t response_length =
      ntag424_send_apdu(reader, session, TI, NTAG424_COM_CLA,
                        NTAG424_COM_CHANGEKEY, 0x00, 0x00, cmd_header,
                        sizeof(cmd_header), keydata, keydata_length, 0,
                        NTAG424_COMM_MODE_FULL, result, sizeof(result));
  return ntag424_status_ok(result, response_length, 0x91, 0x00);
}

uint8_t ntag424_GetCardUID(NTAG424_Reader *reader,
                           ntag424_SessionType *session, const uint8_t *TI,
                           uint8_t *buffer) {
  return ntag424_read_simple_full_response(reader, NTAG424_CMD_GETCARDUUID,
                                           session, TI, buffer, 34);
}

bool ntag424_GetKeyVersion(NTAG424_Reader *reader, ntag424_SessionType *session,
                           const uint8_t *TI, uint8_t keyno,
                           uint8_t *version) {
  if (version == nullptr) {
    return false;
  }

  uint8_t result[16] = {0};
  const uint8_t cmd_header[1] = {keyno};
  const uint8_t response_length =
      ntag424_send_apdu(reader, session, TI, NTAG424_COM_CLA, 0x64, 0x00, 0x00,
                        cmd_header, sizeof(cmd_header), nullptr, 0, 0,
                        NTAG424_COMM_MODE_PLAIN, result, sizeof(result));
  if (response_length < 3 || !ntag424_plain_status_ok(result, response_length,
                                                      0x91, 0x00)) {
    return false;
  }

  *version = result[0];
  return true;
}

uint8_t ntag424_GetTTStatus(NTAG424_Reader *reader,
                            ntag424_SessionType *session, const uint8_t *TI,
                            uint8_t *buffer) {
  return ntag424_read_simple_full_response(reader, NTAG424_CMD_GETTTSTATUS,
                                           session, TI, buffer, 32);
}

uint8_t ntag424_ReadSig(NTAG424_Reader *reader, ntag424_SessionType *session,
                        const uint8_t *TI, uint8_t *buffer) {
  (void)reader;
  (void)session;
  (void)TI;
  (void)buffer;
  return 0;
}

uint8_t ntag424_ReadData(NTAG424_Reader *reader, uint8_t *buffer, int fileno,
                         int offset, int size) {
  if (reader == nullptr || buffer == nullptr) {
    return 0;
  }

  const uint8_t apdu[] = {NTAG424_COM_CLA,
                          NTAG424_CMD_READDATA,
                          0x00,
                          0x00,
                          0x07,
                          static_cast<uint8_t>(fileno),
                          static_cast<uint8_t>(offset & 0xFF),
                          static_cast<uint8_t>((offset >> 8) & 0xFF),
                          static_cast<uint8_t>((offset >> 16) & 0xFF),
                          static_cast<uint8_t>(size & 0xFF),
                          static_cast<uint8_t>((size >> 8) & 0xFF),
                          static_cast<uint8_t>((size >> 16) & 0xFF),
                          0x00};
  uint8_t response[kNtag424MaxApduSize] = {0};
  const uint8_t response_length =
      reader->transceive(apdu, sizeof(apdu), response, sizeof(response));
  if (!ntag424_status_ok(response, response_length, 0x91, 0x00)) {
    return 0;
  }

  const uint8_t data_length = ntag424_response_data_length(response_length);
  memcpy(buffer, response, data_length);
  return data_length;
}

uint8_t ntag424_isNTAG424(NTAG424_Reader *reader,
                          ntag424_SessionType *session,
                          ntag424_VersionInfoType *version_info) {
  if (ntag424_GetVersion(reader, version_info) == 0) {
    return 0;
  }

  if (version_info->HWType == NTAG424_RESPONE_GETVERSION_HWTYPE_NTAG424) {
    if (session != nullptr) {
      session->authenticated = false;
      session->cmd_counter = 0;
    }
    return 1;
  }

  return 0;
}

uint8_t ntag424_GetVersion(NTAG424_Reader *reader,
                           ntag424_VersionInfoType *version_info) {
  if (reader == nullptr || version_info == nullptr) {
    return 0;
  }

  const uint8_t get_version_frame[] = {NTAG424_COM_CLA, NTAG424_CMD_GETVERSION,
                                       0x00,            0x00,
                                       0x00};
  uint8_t frame1[16] = {0};
  const uint8_t frame1_length = reader->transceive(
      get_version_frame, sizeof(get_version_frame), frame1, sizeof(frame1));
  if (!ntag424_status_ok(frame1, frame1_length, 0x91, 0xAF) ||
      ntag424_response_data_length(frame1_length) < 7) {
    return 0;
  }

  version_info->VendorID = frame1[0];
  version_info->HWType = frame1[1];
  version_info->HWSubType = frame1[2];
  version_info->HWMajorVersion = frame1[3];
  version_info->HWMinorVersion = frame1[4];
  version_info->HWStorageSize = frame1[5];
  version_info->HWProtocol = frame1[6];

  const uint8_t next_frame[] = {NTAG424_COM_CLA, NTAG424_CMD_NEXTFRAME, 0x00,
                                0x00,            0x00};
  uint8_t frame2[16] = {0};
  const uint8_t frame2_length =
      reader->transceive(next_frame, sizeof(next_frame), frame2, sizeof(frame2));
  if (!ntag424_status_ok(frame2, frame2_length, 0x91, 0xAF) ||
      ntag424_response_data_length(frame2_length) < 7) {
    return 0;
  }

  version_info->VendorID = frame2[0];
  version_info->SWType = frame2[1];
  version_info->SWSubType = frame2[2];
  version_info->SWMajorVersion = frame2[3];
  version_info->SWMinorVersion = frame2[4];
  version_info->SWStorageSize = frame2[5];
  version_info->SWProtocol = frame2[6];

  uint8_t frame3[24] = {0};
  const uint8_t frame3_length =
      reader->transceive(next_frame, sizeof(next_frame), frame3, sizeof(frame3));
  if (!ntag424_status_ok(frame3, frame3_length, 0x91, 0x00) ||
      ntag424_response_data_length(frame3_length) < 14) {
    return 0;
  }

  memcpy(version_info->UID, frame3, sizeof(version_info->UID));
  memcpy(version_info->BatchNo, frame3 + 7, sizeof(version_info->BatchNo));
  memset(version_info->FabKey, 0, sizeof(version_info->FabKey));
  version_info->CWProd = frame3[12];
  version_info->YearProd = frame3[13];
  version_info->FabKeyID = 0;

  return version_info->HWType == NTAG424_RESPONE_GETVERSION_HWTYPE_NTAG424;
}

bool ntag424_FormatNDEF(NTAG424_Reader *reader) {
  if (reader == nullptr) {
    return false;
  }

  uint8_t ndefdata[54] = {0};
  const uint8_t cmd_header[1] = {0x00};
  uint8_t result[12] = {0};
  bool ret = true;
  const uint8_t memsize = 248;
  uint8_t offset = 0;
  uint8_t datalen = sizeof(ndefdata);
  for (int i = 0; i < memsize; i += sizeof(ndefdata)) {
    if ((offset + datalen) > memsize) {
      datalen = memsize - offset;
    }

    const uint8_t bytesread =
        ntag424_send_apdu(reader, nullptr, nullptr, NTAG424_COM_ISOCLA,
                          NTAG424_CMD_ISOUPDATEBINARY, 0x84, offset, cmd_header,
                          0, ndefdata, datalen, 0, NTAG424_COMM_MODE_PLAIN,
                          result, sizeof(result));
    if (!ntag424_plain_command_succeeded(result, bytesread)) {
      ret = false;
    }
    offset += datalen;
  }
  return ret;
}

bool ntag424_ISOUpdateBinary(NTAG424_Reader *reader, uint8_t *data_to_write,
                             uint8_t length) {
  if (reader == nullptr || data_to_write == nullptr) {
    return false;
  }

  const uint8_t cmd_header[1] = {0x00};
  uint8_t result[12] = {0};
  uint8_t offset = 0;
  uint8_t datalen = 54;
  for (int i = 0; i < length; i += datalen) {
    if ((offset + datalen) > length) {
      datalen = length - offset;
    }
    if (datalen > 0) {
      const uint8_t bytesread = ntag424_send_apdu(
          reader, nullptr, nullptr, NTAG424_COM_ISOCLA,
          NTAG424_CMD_ISOUPDATEBINARY, 0x84, offset, cmd_header, 0,
          data_to_write + offset, datalen, 0, NTAG424_COMM_MODE_PLAIN, result,
          sizeof(result));
      if (!ntag424_plain_command_succeeded(result, bytesread)) {
        return false;
      }
    }
    offset += datalen;
  }
  return true;
}

bool ntag424_ISOSelectFileById(NTAG424_Reader *reader,
                               ntag424_SessionType *session, const uint8_t *TI,
                               int fileid) {
  uint8_t cmd_data[2] = {static_cast<uint8_t>((fileid >> 8) & 0xFF),
                         static_cast<uint8_t>(fileid & 0xFF)};
  return ntag424_iso_select_file(reader, 0x00, cmd_data, sizeof(cmd_data),
                                 session, TI);
}

bool ntag424_ISOSelectFileByDFN(NTAG424_Reader *reader,
                                ntag424_SessionType *session,
                                const uint8_t *TI, uint8_t *dfn) {
  return ntag424_iso_select_file(reader, 0x04, dfn, 7, session, TI);
}

bool ntag424_ISOSelectCCFile(NTAG424_Reader *reader,
                             ntag424_SessionType *session, const uint8_t *TI) {
  uint8_t cmd_data[2] = {0xE1, 0x03};
  return ntag424_iso_select_file(reader, 0x00, cmd_data, sizeof(cmd_data),
                                 session, TI);
}

bool ntag424_ISOSelectNDEFFile(NTAG424_Reader *reader,
                               ntag424_SessionType *session,
                               const uint8_t *TI) {
  uint8_t dfn[sizeof(kIsoApplicationDfn)] = {0};
  memcpy(dfn, kIsoApplicationDfn, sizeof(dfn));
  uint8_t ndef_file[2] = {0xE1, 0x04};
  return ntag424_iso_select_file(reader, 0x04, dfn, sizeof(dfn), session, TI) &&
         ntag424_iso_select_file(reader, 0x00, ndef_file, sizeof(ndef_file),
                                 session, TI);
}

int16_t ntag424_ReadNDEFMessage(NTAG424_Reader *reader, uint8_t *buffer,
                                uint16_t maxsize) {
  if (buffer == nullptr || maxsize == 0) {
    return -1;
  }

  if (!ntag424_ISOSelectNDEFFile(reader, nullptr, nullptr)) {
    return -1;
  }

  uint8_t nlen_response[8] = {0};
  const uint8_t nlen_resp_len =
      ntag424_ISOReadBinary(reader, 0, 2, nlen_response, sizeof(nlen_response));
  if (nlen_resp_len < 4 ||
      !ntag424_plain_status_ok(nlen_response, nlen_resp_len, 0x90, 0x00)) {
    return -1;
  }

  const uint16_t nlen = (static_cast<uint16_t>(nlen_response[0]) << 8) |
                        nlen_response[1];
  if (nlen == 0) {
    return 0;
  }

  uint16_t total_to_read = nlen;
  if (total_to_read > maxsize) {
    total_to_read = maxsize;
  }

  uint16_t total_read = 0;
  while (total_read < total_to_read) {
    uint8_t chunk = total_to_read - total_read;
    if (chunk > 48) {
      chunk = 48;
    }

    uint8_t page_response[56] = {0};
    const uint8_t page_resp_len = ntag424_ISOReadBinary(
        reader, 2 + total_read, chunk, page_response, sizeof(page_response));
    if (page_resp_len < 3 ||
        !ntag424_plain_status_ok(page_response, page_resp_len, 0x90, 0x00)) {
      return -1;
    }

    const uint8_t data_len = page_resp_len - 2;
    memcpy(buffer + total_read, page_response, data_len);
    total_read += data_len;
  }

  return total_read;
}

uint8_t ntag424_ISOReadFile(NTAG424_Reader *reader, uint8_t *buffer,
                            int maxsize) {
  if (reader == nullptr || buffer == nullptr) {
    return 0;
  }

  const uint8_t get_file_settings[] = {NTAG424_COM_CLA,
                                       NTAG424_CMD_GETFILESETTINGS,
                                       0x00,
                                       0x00,
                                       0x01,
                                       0x02,
                                       0x00};
  uint8_t file_settings_response[16] = {0};
  if (reader->transceive(get_file_settings, sizeof(get_file_settings),
                         file_settings_response,
                         sizeof(file_settings_response)) == 0) {
    return 0;
  }

  const uint8_t select_dfn[] = {NTAG424_COM_ISOCLA,
                                NTAG424_CMD_ISOSELECTFILE,
                                0x04,
                                0x00,
                                0x07,
                                0xD2,
                                0x76,
                                0x00,
                                0x00,
                                0x85,
                                0x01,
                                0x01,
                                0x00};
  uint8_t select_dfn_response[16] = {0};
  const uint8_t select_dfn_length = reader->transceive(
      select_dfn, sizeof(select_dfn), select_dfn_response,
      sizeof(select_dfn_response));
  if (!ntag424_status_ok(select_dfn_response, select_dfn_length, 0x90, 0x00)) {
    return 0;
  }

  const uint8_t select_file[] = {NTAG424_COM_ISOCLA,
                                 NTAG424_CMD_ISOSELECTFILE,
                                 0x00,
                                 0x00,
                                 0x02,
                                 0xE1,
                                 0x04,
                                 0x00};
  uint8_t select_file_response[16] = {0};
  const uint8_t select_file_length = reader->transceive(
      select_file, sizeof(select_file), select_file_response,
      sizeof(select_file_response));
  if (!ntag424_status_ok(select_file_response, select_file_length, 0x90, 0x00)) {
    return 0;
  }

  const uint8_t size_read[] = {NTAG424_COM_ISOCLA, NTAG424_CMD_ISOREADBINARY,
                               0x00,               0x00,
                               0x03};
  uint8_t size_response[8] = {0};
  const uint8_t size_length = reader->transceive(
      size_read, sizeof(size_read), size_response, sizeof(size_response));
  if (!ntag424_status_ok(size_response, size_length, 0x90, 0x00) ||
      ntag424_response_data_length(size_length) < 2) {
    return 0;
  }

  int filesize = static_cast<int>(size_response[1]) - 5;
  uint8_t pagesize = 32;
  const uint8_t pages = (filesize / pagesize) + 1;
  uint8_t offset = 0;
  bool early_out = false;
  for (int i = 0; i < pages; i++) {
    offset = i * pagesize;
    if ((offset + pagesize) > filesize) {
      pagesize = filesize - offset;
    }
    if ((offset + pagesize) > maxsize) {
      pagesize = maxsize - offset;
      filesize = maxsize;
      early_out = true;
    }

    const uint8_t read_binary[] = {NTAG424_COM_ISOCLA,
                                   NTAG424_CMD_ISOREADBINARY,
                                   0x00,
                                   static_cast<uint8_t>(7 + offset),
                                   pagesize};
    uint8_t page_response[64] = {0};
    const uint8_t page_length = reader->transceive(
        read_binary, sizeof(read_binary), page_response, sizeof(page_response));
    if (!ntag424_status_ok(page_response, page_length, 0x90, 0x00)) {
      return 0;
    }

    memcpy(buffer + offset, page_response, pagesize);
    if (early_out) {
      break;
    }
  }

  return filesize;
}

uint8_t ntag424_ISOReadBinary(NTAG424_Reader *reader, uint16_t offset,
                              uint8_t le, uint8_t *response,
                              uint16_t response_bufsize) {
  if (reader == nullptr || response == nullptr || response_bufsize == 0) {
    return 0;
  }

  const uint8_t apdu[] = {NTAG424_COM_ISOCLA,
                          NTAG424_CMD_ISOREADBINARY,
                          static_cast<uint8_t>((offset >> 8) & 0xFF),
                          static_cast<uint8_t>(offset & 0xFF),
                          le};
  const uint8_t max_length =
      response_bufsize > 255 ? 255 : static_cast<uint8_t>(response_bufsize);
  return reader->transceive(apdu, sizeof(apdu), response, max_length);
}
