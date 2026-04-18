#include <cstdint>

void delay(unsigned long) {}

long random(long max) {
  return max > 0 ? 4 % max : 0;
}
