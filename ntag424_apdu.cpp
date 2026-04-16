#include "ntag424_apdu.h"

#include <string.h>

namespace {

constexpr uint8_t kNtag424MaxApduSize = 80;
constexpr uint8_t kNtag424Plain = 0x00;
constexpr uint8_t kNtag424Mac = 0x01;
constexpr uint8_t kNtag424Full = 0x02;
constexpr uint8_t kNtag424ComCla = 0x90;
constexpr uint8_t kNtag424ComIsoCla = 0x00;
constexpr uint8_t kNtag424CmdIsoSelectFile = 0xA4;
constexpr uint8_t kNtag424CmdIsoUpdateBinary = 0xD6;

bool ntag424_copy_input_if_present(const uint8_t *input, uint8_t input_length,
                                   uint8_t *output, uint8_t output_capacity,
                                   uint8_t &offset) {
  if (input_length == 0) {
    return true;
  }

  if (input == nullptr || output == nullptr ||
      offset > static_cast<uint8_t>(output_capacity - input_length)) {
    return false;
  }

  memcpy(output + offset, input, input_length);
  offset += input_length;
  return true;
}

}

bool ntag424_response_has_status(const uint8_t *response,
                                 uint8_t response_length, uint8_t sw1,
                                 uint8_t sw2) {
  return response != nullptr && response_length >= 2 &&
         response[response_length - 2] == sw1 &&
         response[response_length - 1] == sw2;
}

bool ntag424_plain_command_succeeded(const uint8_t *response,
                                     uint8_t response_length) {
  return ntag424_response_has_status(response, response_length, 0x90, 0x00);
}

uint8_t ntag424_copy_response_data_if_status(const uint8_t *response,
                                             uint8_t response_length,
                                             uint8_t sw1, uint8_t sw2,
                                             uint8_t *buffer) {
  if (!ntag424_response_has_status(response, response_length, sw1, sw2)) {
    return 0;
  }

  const uint8_t data_length = response_length - 2;
  if (data_length > 0 && buffer != nullptr) {
    memcpy(buffer, response, data_length);
  }
  return data_length;
}

uint8_t ntag424_build_apdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                           const uint8_t *cmd_header, uint8_t cmd_header_len,
                           const uint8_t *cmd_data, uint8_t cmd_data_len,
                           uint8_t le, uint8_t comm_mode,
                           ntag424_SessionType *session, const uint8_t *TI,
                           uint8_t *apdu_out) {
  if (apdu_out == nullptr) {
    return 0;
  }

  if ((comm_mode == kNtag424Mac || comm_mode == kNtag424Full) &&
      (session == nullptr || TI == nullptr || !session->authenticated)) {
    return 0;
  }

  apdu_out[0] = cla;
  apdu_out[1] = ins;
  apdu_out[2] = p1;
  apdu_out[3] = p2;
  const uint8_t offset_lc = 4;
  apdu_out[offset_lc] = cmd_data_len + cmd_header_len;
  uint8_t offset = 5;

  if (!ntag424_copy_input_if_present(cmd_header, cmd_header_len, apdu_out,
                                     kNtag424MaxApduSize, offset)) {
    return 0;
  }

  if (comm_mode == kNtag424Plain) {
    if (!ntag424_copy_input_if_present(cmd_data, cmd_data_len, apdu_out,
                                       kNtag424MaxApduSize, offset)) {
      return 0;
    }
  } else if (comm_mode == kNtag424Mac) {
    if (!ntag424_copy_input_if_present(cmd_data, cmd_data_len, apdu_out,
                                       kNtag424MaxApduSize, offset)) {
      return 0;
    }

    uint8_t cmac_short[8];
    uint8_t ins_byte[1] = {ins};
    ntag424_MAC(session, ins_byte, const_cast<uint8_t *>(cmd_header),
                cmd_header_len, const_cast<uint8_t *>(cmd_data), cmd_data_len,
                cmac_short);
    if (!ntag424_copy_input_if_present(cmac_short, sizeof(cmac_short), apdu_out,
                                       kNtag424MaxApduSize, offset)) {
      return 0;
    }
    apdu_out[offset_lc] += sizeof(cmac_short);
  } else if (comm_mode == kNtag424Full) {
    uint8_t cmac_short[8];
    if (cmd_data_len > 0) {
      uint8_t payload_padded[kNtag424MaxApduSize];
      memcpy(payload_padded, cmd_data, cmd_data_len);
      const uint8_t padded_payload_length =
          ntag424_addpadding(cmd_data_len, 16, payload_padded);
      if (padded_payload_length == 0 ||
          padded_payload_length > kNtag424MaxApduSize) {
        return 0;
      }

      uint8_t iv[16];
      uint8_t ive[16];
      iv[0] = 0xA5;
      iv[1] = 0x5A;
      memcpy(iv + 2, TI, NTAG424_AUTHRESPONSE_TI_SIZE);
      iv[6] = session->cmd_counter & 0xff;
      iv[7] = (session->cmd_counter >> 8) & 0xff;
      memset(iv + 8, 0, 8);
      ntag424_encrypt(session->session_key_enc, 16, iv, ive);

      uint8_t payload_encrypted[kNtag424MaxApduSize];
      ntag424_encrypt(session->session_key_enc, ive, padded_payload_length,
                      payload_padded, payload_encrypted);
      if (!ntag424_copy_input_if_present(payload_encrypted,
                                         padded_payload_length, apdu_out,
                                         kNtag424MaxApduSize, offset)) {
        return 0;
      }

      uint8_t ins_byte[1] = {ins};
      ntag424_MAC(session, session->session_key_mac, ins_byte,
                  const_cast<uint8_t *>(cmd_header), cmd_header_len,
                  payload_encrypted, padded_payload_length, cmac_short);
      if (!ntag424_copy_input_if_present(cmac_short, sizeof(cmac_short),
                                         apdu_out, kNtag424MaxApduSize,
                                         offset)) {
        return 0;
      }
      apdu_out[offset_lc] =
          cmd_header_len + padded_payload_length + sizeof(cmac_short);
    } else {
      uint8_t ins_byte[1] = {ins};
      ntag424_MAC(session, session->session_key_mac, ins_byte,
                  const_cast<uint8_t *>(cmd_header), cmd_header_len,
                  const_cast<uint8_t *>(cmd_data), cmd_data_len, cmac_short);
      if (!ntag424_copy_input_if_present(cmac_short, sizeof(cmac_short),
                                         apdu_out, kNtag424MaxApduSize,
                                         offset)) {
        return 0;
      }
      apdu_out[offset_lc] += sizeof(cmac_short);
    }
  }

  if (ins != kNtag424CmdIsoUpdateBinary) {
    if (offset >= kNtag424MaxApduSize) {
      return 0;
    }
    apdu_out[offset] = le;
    offset++;
  }

  return offset;
}

uint8_t ntag424_process_response(const uint8_t *response,
                                 uint8_t response_length, uint8_t comm_mode,
                                 ntag424_SessionType *session,
                                 const uint8_t *TI, uint8_t *processed_out) {
  if (response == nullptr || processed_out == nullptr) {
    return 0;
  }

  if ((comm_mode == kNtag424Mac || comm_mode == kNtag424Full) &&
      (session == nullptr || TI == nullptr)) {
    return 0;
  }

  if (response_length > kNtag424MaxApduSize) {
    return 0;
  }

  if ((response_length >= 10) &&
      (comm_mode == kNtag424Full || comm_mode == kNtag424Mac)) {
    uint8_t respcmac[8];
    memcpy(respcmac, response + (response_length - 10), sizeof(respcmac));

    uint8_t checkmacin[kNtag424MaxApduSize + 6];
    uint16_t resp_counter = session->cmd_counter + 1;
    checkmacin[0] = response[response_length - 1];
    checkmacin[1] = resp_counter & 0xff;
    checkmacin[2] = (resp_counter >> 8) & 0xff;
    memcpy(checkmacin + 3, TI, NTAG424_AUTHRESPONSE_TI_SIZE);

    uint8_t padded_respdata_length = 0;
    if (response_length > 10) {
      padded_respdata_length = response_length - 10;
      memcpy(checkmacin + 3 + NTAG424_AUTHRESPONSE_TI_SIZE, response,
             padded_respdata_length);
    }

    const uint8_t maclength =
        3 + NTAG424_AUTHRESPONSE_TI_SIZE + padded_respdata_length;
    uint8_t checkmac[8];
    ntag424_cmac_short(session->session_key_mac, checkmacin, maclength,
                       checkmac);
    for (uint8_t i = 0; i < sizeof(respcmac); i++) {
      if (respcmac[i] != checkmac[i]) {
        return 0;
      }
    }
  }

  uint8_t processed_length = response_length;
  if ((response_length > 10) && (comm_mode == kNtag424Full)) {
    uint8_t ivd[16];
    uint8_t ivde[16];
    ivd[0] = 0x5A;
    ivd[1] = 0xA5;
    memcpy(ivd + 2, TI, NTAG424_AUTHRESPONSE_TI_SIZE);
    ivd[6] = (session->cmd_counter + 1) & 0xff;
    ivd[7] = ((session->cmd_counter + 1) >> 8) & 0xff;
    memset(ivd + 8, 0, 8);
    ntag424_encrypt(session->session_key_enc, 16, ivd, ivde);

    const uint8_t encrypted_length = response_length - 10;
    uint8_t respplain[kNtag424MaxApduSize];
    if (!ntag424_decrypt(session->session_key_enc, ivde, encrypted_length,
                         const_cast<uint8_t *>(response), respplain)) {
      return 0;
    }

    memcpy(processed_out, respplain, encrypted_length);
    uint8_t resp_no_padding = encrypted_length;
    for (int i = encrypted_length - 1; i >= 0; i--) {
      if (processed_out[i] == 0x00) {
        resp_no_padding = i;
      } else if (processed_out[i] == 0x80) {
        resp_no_padding = i;
        break;
      } else {
        break;
      }
    }

    processed_out[resp_no_padding] = response[response_length - 2];
    processed_out[resp_no_padding + 1] = response[response_length - 1];
    processed_length = resp_no_padding + 2;
  } else {
    memcpy(processed_out, response, response_length);
  }

  if (session != nullptr) {
    session->cmd_counter += 1;
  }
  return processed_length;
}

uint8_t ntag424_read_simple_full_response(NTAG424_Reader *reader,
                                          uint8_t command,
                                          ntag424_SessionType *session,
                                          const uint8_t *TI, uint8_t *buffer,
                                          uint8_t result_size) {
  (void)result_size;
  if (reader == nullptr) {
    return 0;
  }

  uint8_t apdu[kNtag424MaxApduSize];
  const uint8_t apdu_length =
      ntag424_build_apdu(kNtag424ComCla, command, 0x00, 0x00, nullptr, 0,
                         nullptr, 0, 0, kNtag424Full, session, TI, apdu);
  if (apdu_length == 0) {
    return 0;
  }

  uint8_t response[34];
  const uint8_t response_length =
      reader->transceive(apdu, apdu_length, response, sizeof(response));
  if (response_length == 0) {
    return 0;
  }

  uint8_t processed[34];
  const uint8_t processed_length = ntag424_process_response(
      response, response_length, kNtag424Full, session, TI, processed);
  return ntag424_copy_response_data_if_status(processed, processed_length, 0x91,
                                              0x00, buffer);
}

bool ntag424_iso_select_file(NTAG424_Reader *reader, uint8_t p1_value,
                             uint8_t *cmd_data, uint8_t cmd_data_length,
                             ntag424_SessionType *session,
                             const uint8_t *TI) {
  if (reader == nullptr) {
    return false;
  }

  uint8_t apdu[kNtag424MaxApduSize];
  const uint8_t apdu_length =
      ntag424_build_apdu(kNtag424ComIsoCla, kNtag424CmdIsoSelectFile,
                         p1_value, 0x00, nullptr, 0, cmd_data,
                         cmd_data_length, 0, kNtag424Plain, session, TI, apdu);
  if (apdu_length == 0) {
    return false;
  }

  uint8_t result[12];
  const uint8_t response_length =
      reader->transceive(apdu, apdu_length, result, sizeof(result));
  if (response_length == 0) {
    return false;
  }

  uint8_t processed[12];
  const uint8_t processed_length = ntag424_process_response(
      result, response_length, kNtag424Plain, session, TI, processed);
  return ntag424_plain_command_succeeded(processed, processed_length);
}

bool ntag424_plain_status_ok(const uint8_t *response, uint8_t response_length,
                             uint8_t sw1, uint8_t sw2) {
  return ntag424_response_has_status(response, response_length, sw1, sw2);
}
