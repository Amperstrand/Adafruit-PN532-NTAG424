#ifndef TINYCMAC_H
#define TINYCMAC_H
#if __has_include("Arduino.h")
#include "Arduino.h"
#else
#include <cstddef>
#include <cstdint>
#endif
#include "mbedtls/aes.h"

void xorBlock(uint8_t *output, const uint8_t *input1, const uint8_t *input2,
              size_t len);

void AES128_CMAC(const uint8_t *key, const uint8_t *input, size_t length,
                 uint8_t output[16]);

void aes_encrypt(uint8_t *key, uint8_t *iv, uint8_t length, uint8_t *input,
                 uint8_t *output);
#endif
