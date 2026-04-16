#include "ntag424_crypto.h"

#include "mbedtls/aes.h"
#include <stdlib.h>
#include <string.h>

#if __has_include("Arduino.h")
#include "aescmac.h"
#else
void AES128_CMAC(const uint8_t *key, const uint8_t *input, size_t length,
                 uint8_t output[16]);
long random(long max);
#endif

#if defined(ARDUINO) && __has_include(<Arduino_CRC32.h>)
#include <Arduino_CRC32.h>
#else
class Arduino_CRC32 {
public:
  uint32_t calc(const uint8_t *data, size_t datalength) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < datalength; ++i) {
      crc ^= data[i];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint32_t mask = static_cast<uint32_t>(-(crc & 1u));
        crc = (crc >> 1) ^ (0xEDB88320u & mask);
      }
    }
    return crc ^ 0xFFFFFFFFu;
  }
};
#endif

#if defined(ESP32) && __has_include("esp_random.h")
#include "esp_random.h"
#endif

void ntag424_random(uint8_t *output, uint8_t bytecount) {
  for (int i = 0; i < bytecount; i++) {
#ifdef ESP32
    output[i] = esp_random() & 0xFF;
#else
    output[i] = random(256);
#endif
  }
}

uint8_t ntag424_addpadding(uint8_t inputlength, uint8_t paddinglength,
                           uint8_t *buffer) {
  uint8_t zeroestoadd = paddinglength - (inputlength % paddinglength);
  memset(buffer + inputlength, 0, zeroestoadd);
  if (zeroestoadd > 0) {
    buffer[inputlength] = 0x80;
  }
  return inputlength + zeroestoadd;
}

uint8_t ntag424_encrypt(uint8_t *key, uint8_t length, uint8_t *input,
                        uint8_t *output) {
  uint8_t iv[16];
  memset(iv, 0, sizeof(iv));
  return ntag424_encrypt(key, iv, length, input, output);
}

uint8_t ntag424_encrypt(uint8_t *key, uint8_t *iv, uint8_t length,
                        uint8_t *input, uint8_t *output) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_enc(&ctx, key, 128) != 0) {
    mbedtls_aes_free(&ctx);
    return 0;
  }
  mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, length, iv, (uint8_t *)input,
                        (uint8_t *)output);
  mbedtls_aes_free(&ctx);
  return 1;
}

uint8_t ntag424_decrypt(uint8_t *key, uint8_t length, uint8_t *input,
                        uint8_t *output) {
  uint8_t iv[16];
  memset(iv, 0, sizeof(iv));
  return ntag424_decrypt(key, iv, length, input, output);
}

uint8_t ntag424_decrypt(uint8_t *key, uint8_t *iv, uint8_t length,
                        uint8_t *input, uint8_t *output) {
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_dec(&ctx, key, 128) != 0) {
    mbedtls_aes_free(&ctx);
    return 0;
  }
  if (mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, length, iv,
                            (uint8_t *)input, (uint8_t *)output) != 0) {
    return 0;
  }
  mbedtls_aes_free(&ctx);
  return 1;
}

uint8_t ntag424_cmac_short(uint8_t *key, uint8_t *input, uint8_t length,
                           uint8_t *cmac) {
  uint8_t regularcmac[16];
  ntag424_cmac(key, input, length, regularcmac);
  uint8_t c = 0;
  for (int i = 1; i < 16; i += 2) {
    cmac[c] = regularcmac[i];
    c++;
  }

#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("INPUT: "));
  PrintHexChar(input, length);
  PN532DEBUGPRINT.print(F("CMAC: "));
  PrintHexChar(regularcmac, 16);
  PN532DEBUGPRINT.print(F("CMAC_SHORT: "));
  PrintHexChar(cmac, 8);
#endif
  return 0;
}

uint8_t ntag424_cmac(uint8_t *key, uint8_t *input, uint8_t length,
                     uint8_t *cmac) {
  AES128_CMAC(key, input, length, cmac);
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("cmac key: "));
  PrintHexChar(key, 16);
  PN532DEBUGPRINT.print(F("cmac input: "));
  PrintHexChar(input, length);
  PN532DEBUGPRINT.print(F("cmac output: "));
  PrintHexChar(cmac, 16);
#endif
  return 0;
}

uint8_t ntag424_MAC(ntag424_SessionType *session, uint8_t *cmd,
                    uint8_t *cmdheader, uint8_t cmdheader_length,
                    uint8_t *cmddata, uint8_t cmddata_length,
                    uint8_t *signature) {
  return ntag424_MAC(session, session->session_key_mac, cmd, cmdheader,
                     cmdheader_length, cmddata, cmddata_length, signature);
}

uint8_t ntag424_MAC(ntag424_SessionType *session, uint8_t *key, uint8_t *cmd,
                    uint8_t *cmdheader, uint8_t cmdheader_length,
                    uint8_t *cmddata, uint8_t cmddata_length,
                    uint8_t *signature) {
  int16_t intcounter = session->cmd_counter;
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("cmdheader_length: "));
  Serial.println(cmdheader_length);
  PN532DEBUGPRINT.print(F("cmddata_length: "));
  Serial.println(cmddata_length);
  PN532DEBUGPRINT.print(F("cmdheader: "));
  PrintHexChar(cmdheader, cmdheader_length);
  PN532DEBUGPRINT.print(F("cmddata: "));
  PrintHexChar(cmddata, cmddata_length);
#endif
  uint8_t counter_l = (uint8_t)(intcounter & 0xff);
  uint8_t counter_u = (uint8_t)((intcounter >> 8) & 0xff);
  uint8_t ntcounter[2] = {counter_l, counter_u};

#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("mesglen2: "));
  PrintHexChar(ntcounter, sizeof(ntcounter));
#endif
  uint8_t msglen = 1 + sizeof(ntcounter) + NTAG424_AUTHRESPONSE_TI_SIZE +
                   cmdheader_length + cmddata_length;
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("mesglen2: "));
  Serial.println(msglen);
#endif
  uint8_t mesg[48];
  if (msglen > sizeof(mesg)) return 0;

  int dataoffset = 0;
  mesg[dataoffset] = cmd[0];
  dataoffset++;
  memcpy(mesg + dataoffset, ntcounter, sizeof(ntcounter));
  dataoffset += sizeof(ntcounter);
  memcpy(mesg + dataoffset, ntag424_authresponse_TI,
         NTAG424_AUTHRESPONSE_TI_SIZE);
  dataoffset += NTAG424_AUTHRESPONSE_TI_SIZE;
  memcpy(mesg + dataoffset, cmdheader, cmdheader_length);
  if (cmddata_length > 0) {
    dataoffset += cmdheader_length;
    memcpy(mesg + dataoffset, cmddata, cmddata_length);
  }
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("mesg: padded: "));
  PrintHexChar(mesg, msglen);
#endif
  ntag424_cmac_short(key, mesg, msglen, signature);
  return 0;
}

void ntag424_derive_session_keys(ntag424_SessionType *session, uint8_t *key,
                                 uint8_t *RndA, uint8_t *RndB) {
  uint8_t f1[2];
  uint8_t f2[6];
  uint8_t f3[6];
  uint8_t f4[10];
  uint8_t f5[8];
  uint8_t f2xor3[6];
  memcpy(&f1, RndA, 2);
  memcpy(&f2, RndA + 2, 6);
  memcpy(&f3, RndB, 6);
  memcpy(&f4, RndB + 6, 10);
  memcpy(&f5, RndA + 8, 8);
  for (int i = 0; i < 6; ++i) {
    f2xor3[i] = f2[i] ^ f3[i];
  }
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.println(F("DERIVE SESSIONKEYS: "));
  PN532DEBUGPRINT.print(F("RndA: "));
  PrintHexChar(RndA, 16);
  PN532DEBUGPRINT.print(F("RndB: "));
  PrintHexChar(RndB, 16);
  PN532DEBUGPRINT.print(F("f1: "));
  PrintHexChar(f1, 2);
  PN532DEBUGPRINT.print(F("f2: "));
  PrintHexChar(f2, 6);
  PN532DEBUGPRINT.print(F("f3: "));
  PrintHexChar(f3, 6);
  PN532DEBUGPRINT.print(F("f4: "));
  PrintHexChar(f4, 10);
  PN532DEBUGPRINT.print(F("f5: "));
  PrintHexChar(f5, 8);
  PN532DEBUGPRINT.print(F("f2xorf3: "));
  PrintHexChar(f2xor3, 6);
#endif
  uint8_t sv1[32] = {
      0xA5,  0x5A,      0x00,      0x01,      0x00,      0x80,      f1[0],
      f1[1], f2xor3[0], f2xor3[1], f2xor3[2], f2xor3[3], f2xor3[4], f2xor3[5],
      f4[0], f4[1],     f4[2],     f4[3],     f4[4],     f4[5],     f4[6],
      f4[7], f4[8],     f4[9],     f5[0],     f5[1],     f5[2],     f5[3],
      f5[4], f5[5],     f5[6],     f5[7]};
  uint8_t sv2[32] = {
      0x5A,  0xA5,      0x00,      0x01,      0x00,      0x80,      f1[0],
      f1[1], f2xor3[0], f2xor3[1], f2xor3[2], f2xor3[3], f2xor3[4], f2xor3[5],
      f4[0], f4[1],     f4[2],     f4[3],     f4[4],     f4[5],     f4[6],
      f4[7], f4[8],     f4[9],     f5[0],     f5[1],     f5[2],     f5[3],
      f5[4], f5[5],     f5[6],     f5[7]};
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("SV1: "));
  PrintHexChar(sv1, 32);
  PN532DEBUGPRINT.print(F("SV2: "));
  PrintHexChar(sv2, 32);
#endif

  ntag424_cmac(key, sv1, 32, session->session_key_enc);
  ntag424_cmac(key, sv2, 32, session->session_key_mac);

#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("session_key_mac: "));
  PrintHexChar(session->session_key_mac, NTAG424_SESSION_KEYSIZE);
  PN532DEBUGPRINT.print(F("session_key_enc: "));
  PrintHexChar(session->session_key_enc, NTAG424_SESSION_KEYSIZE);
#endif
}

uint32_t ntag424_crc32(uint8_t *data, uint8_t datalength) {
  Arduino_CRC32 crc32;
#ifdef NTAG424DEBUG
  PrintHexChar((uint8_t const *)data, datalength);
#endif
  uint32_t const crc32_res = crc32.calc((uint8_t const *)data, datalength);
#ifdef NTAG424DEBUG
  Serial.println(crc32_res, HEX);
#endif
  return crc32_res;
}

uint8_t ntag424_rotl(uint8_t *input, uint8_t *output, uint8_t bufferlen,
                     uint8_t rotation) {
  if ((rotation > 16) || (bufferlen < rotation)) {
#ifdef NTAG424DEBUG
    PN532DEBUGPRINT.print(F("rotation-error: overflow or negative rotation"));
#endif
    return 0;
  }

#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("rotation-before: "));
  PrintHexChar(input, bufferlen);
#endif

  for (int i = 0; i < bufferlen; i++) {
    int z = i - rotation;
    if (z < 0) {
      z = bufferlen + z;
    }
    output[z] = input[i];
  }
#ifdef NTAG424DEBUG
  PN532DEBUGPRINT.print(F("rotation-after: "));
  PrintHexChar(output, bufferlen);
#endif
  return 1;
}
