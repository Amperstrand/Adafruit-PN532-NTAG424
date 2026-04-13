#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t seed_value;

static void randomSeed_arduino(uint32_t seed) {
  if (seed == 0) {
    seed = 1;
  }
  seed_value = seed;
}

static uint8_t random_byte_arduino(void) {
  seed_value = seed_value * 1664525UL + 1013904223UL;
  return (uint8_t)((seed_value >> 16) % 256);
}

static void fill_rnda(uint8_t *output, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    output[i] = random_byte_arduino();
  }
}

static void print_rnda(const char *label, const uint8_t *data, size_t len) {
  printf("%s", label);
  for (size_t i = 0; i < len; ++i) {
    printf("%02X", data[i]);
    if (i + 1 < len) {
      printf(" ");
    }
  }
  printf("\n");
}

int main(void) {
  uint8_t rnda_first[16];
  uint8_t rnda_second[16];

  randomSeed_arduino(42);
  fill_rnda(rnda_first, sizeof(rnda_first));

  randomSeed_arduino(42);
  fill_rnda(rnda_second, sizeof(rnda_second));

  print_rnda("RndA #1: ", rnda_first, sizeof(rnda_first));
  print_rnda("RndA #2: ", rnda_second, sizeof(rnda_second));

  if (memcmp(rnda_first, rnda_second, sizeof(rnda_first)) == 0) {
    puts("RESULT: IDENTICAL — same seed produces same RndA");
    return 1;
  }

  puts("RESULT: DIFFERENT — fixed behavior detected");
  return 0;
}
