#ifndef NTAG424_APDU_H
#define NTAG424_APDU_H

#include "ntag424_crypto.h"
#include "ntag424_reader.h"

bool ntag424_response_has_status(const uint8_t *response,
                                 uint8_t response_length, uint8_t sw1,
                                 uint8_t sw2);

bool ntag424_plain_command_succeeded(const uint8_t *response,
                                     uint8_t response_length);

uint8_t ntag424_copy_response_data_if_status(const uint8_t *response,
                                             uint8_t response_length,
                                             uint8_t sw1, uint8_t sw2,
                                             uint8_t *buffer);

uint8_t ntag424_build_apdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                            const uint8_t *cmd_header, uint8_t cmd_header_len,
                            const uint8_t *cmd_data, uint8_t cmd_data_len,
                            uint8_t le, uint8_t comm_mode,
                            ntag424_SessionType *session, uint8_t *apdu_out);

uint8_t ntag424_process_response(const uint8_t *response,
                                 uint8_t response_length, uint8_t comm_mode,
                                 ntag424_SessionType *session,
                                 uint8_t *processed_out);

uint8_t ntag424_read_simple_full_response(NTAG424_Reader *reader,
                                          uint8_t command,
                                          ntag424_SessionType *session,
                                          uint8_t *buffer, uint8_t result_size);

bool ntag424_iso_select_file(NTAG424_Reader *reader, uint8_t p1_value,
                             uint8_t *cmd_data, uint8_t cmd_data_length,
                             ntag424_SessionType *session);

bool ntag424_plain_status_ok(const uint8_t *response, uint8_t response_length,
                             uint8_t sw1, uint8_t sw2);

#endif
