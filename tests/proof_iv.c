#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mbedtls/aes.h>

static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
}

int main(void) {
    uint8_t key[16] = {0x82, 0x48, 0x13, 0x4A, 0x38, 0x6E, 0x86, 0xEB,
                       0x7F, 0xAF, 0x54, 0xA5, 0x2E, 0x53, 0x6C, 0xB6};
    uint8_t ti[4] = {0x7A, 0x21, 0x08, 0x5E};
    uint8_t iv_32[32] = {0};
    uint8_t iv_16[16] = {0};
    uint8_t correct_ecb[16];
    uint8_t buggy_cbc[32];
    uint8_t zero_iv[16] = {0};
    struct {
        uint8_t out[16];
        uint8_t canary[16];
    } overflow_check;
    uint8_t expected_canary[16];
    uint8_t zero_iv_overflow[16] = {0};
    mbedtls_aes_context ctx;

    iv_32[0] = 0xA5;
    iv_32[1] = 0x5A;
    memcpy(iv_32 + 2, ti, 4);
    iv_32[6] = 0x00;
    iv_32[7] = 0x00;
    memset(iv_32 + 8, 0, 24);

    iv_16[0] = 0xA5;
    iv_16[1] = 0x5A;
    memcpy(iv_16 + 2, ti, 4);
    iv_16[6] = 0x00;
    iv_16[7] = 0x00;
    memset(iv_16 + 8, 0, 8);

    mbedtls_aes_init(&ctx);
    if (mbedtls_aes_setkey_enc(&ctx, key, 128) != 0) {
        printf("FAIL: mbedtls_aes_setkey_enc failed\n");
        return 1;
    }

    if (mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, iv_16, correct_ecb) != 0) {
        printf("FAIL: mbedtls_aes_crypt_ecb failed\n");
        mbedtls_aes_free(&ctx);
        return 1;
    }

    if (mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 32, zero_iv, iv_32, buggy_cbc) != 0) {
        printf("FAIL: mbedtls_aes_crypt_cbc failed\n");
        mbedtls_aes_free(&ctx);
        return 1;
    }

    print_hex("Correct 16-byte ECB", correct_ecb, 16);
    print_hex("Buggy CBC first block", buggy_cbc, 16);

    if (memcmp(correct_ecb, buggy_cbc, 16) == 0) {
        printf("PASS: first 16 bytes match the correct IV encryption\n");
    } else {
        printf("FAIL: first 16 bytes do not match the correct IV encryption\n");
        mbedtls_aes_free(&ctx);
        return 1;
    }

    memset(overflow_check.out, 0, sizeof(overflow_check.out));
    memset(overflow_check.canary, 0xCC, sizeof(overflow_check.canary));
    memcpy(expected_canary, overflow_check.canary, sizeof(expected_canary));

    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, 32, zero_iv_overflow, iv_32,
                          overflow_check.out);

    print_hex("Canary after buggy CBC", overflow_check.canary, 16);
    if (memcmp(overflow_check.canary, expected_canary, 16) != 0) {
        printf("PASS: 32-byte CBC write corrupted bytes past the 16-byte buffer\n");
        mbedtls_aes_free(&ctx);
        return 0;
    }

    printf("FAIL: overflow was not detected\n");
    mbedtls_aes_free(&ctx);
    return 1;
}
