#ifndef NTAG424_CHANGEKEY_UTILS_H
#define NTAG424_CHANGEKEY_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static inline uint8_t ntag424_build_changekey_payload(
    const uint8_t *oldkey, const uint8_t *newkey, uint8_t keynumber,
    uint8_t keyversion, uint32_t jamcrc_newkey, uint8_t *keydata) {
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

static inline bool ntag424_response_has_status(const uint8_t *response,
                                               uint8_t response_length,
                                               uint8_t sw1, uint8_t sw2) {
  return response != NULL && response_length >= 2 &&
         response[response_length - 2] == sw1 &&
         response[response_length - 1] == sw2;
}

static inline bool ntag424_changekey_succeeded(const uint8_t *response,
                                               uint8_t response_length) {
  return ntag424_response_has_status(response, response_length, 0x91, 0x00);
}

static inline bool ntag424_plain_command_succeeded(const uint8_t *response,
                                                   uint8_t response_length) {
  return ntag424_response_has_status(response, response_length, 0x90, 0x00);
}

static inline uint8_t ntag424_copy_response_data_if_status(
    const uint8_t *response, uint8_t response_length, uint8_t sw1, uint8_t sw2,
    uint8_t *buffer) {
  if (!ntag424_response_has_status(response, response_length, sw1, sw2)) {
    return 0;
  }

  const uint8_t data_length = response_length - 2;
  if (data_length > 0 && buffer != NULL) {
    memcpy(buffer, response, data_length);
  }
  return data_length;
}

#endif
