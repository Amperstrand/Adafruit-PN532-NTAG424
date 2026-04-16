// NTAG424 cryptographic primitives (reader-agnostic)
#ifndef NTAG424_CRYPTO_H
#define NTAG424_CRYPTO_H
#include <stdint.h>
#include <string.h>

#if __has_include("Arduino.h")
#include "aescmac.h"
#endif

#define NTAG424_AUTHRESPONSE_TI_SIZE 4
#define NTAG424_SESSION_KEYSIZE 16

struct ntag424_SessionType {
  bool authenticated;  ///< true = authenticated
  int16_t cmd_counter; ///< command counter
  uint8_t
      session_key_enc[NTAG424_SESSION_KEYSIZE]; ///< session encryption key
  uint8_t session_key_mac[NTAG424_SESSION_KEYSIZE]; ///< session mac key
}; ///< struct type foir the authentication session data

extern uint8_t ntag424_authresponse_TI[NTAG424_AUTHRESPONSE_TI_SIZE];

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
uint8_t ntag424_MAC(ntag424_SessionType *session, uint8_t *cmd,
                    uint8_t *cmdheader, uint8_t cmdheader_length,
                    uint8_t *cmddata, uint8_t cmddata_length,
                    uint8_t *signature);
uint8_t ntag424_MAC(ntag424_SessionType *session, uint8_t *key, uint8_t *cmd,
                    uint8_t *cmdheader, uint8_t cmdheader_length,
                    uint8_t *cmddata, uint8_t cmddata_length,
                    uint8_t *signature);
void ntag424_random(uint8_t *output, uint8_t bytecount);
void ntag424_derive_session_keys(ntag424_SessionType *session, uint8_t *key,
                                 uint8_t *RndA, uint8_t *RndB);
uint8_t ntag424_rotl(uint8_t *input, uint8_t *output, uint8_t bufferlen,
                     uint8_t rotation);

#endif
