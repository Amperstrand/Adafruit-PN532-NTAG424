#ifndef ARDUINO_CRC32_H
#define ARDUINO_CRC32_H

#include <stdint.h>
#include <stddef.h>
#include <zlib.h>

class Arduino_CRC32 {
public:
  uint32_t calc(const uint8_t *data, size_t length) const {
    return (uint32_t)::crc32(0L, data, (uInt)length);
  }
};

#endif
