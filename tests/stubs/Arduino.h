#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>

#define LOW  0
#define HIGH 1
#define INPUT  0
#define OUTPUT 1

class SerialStub {
public:
  void begin(unsigned long) {}
  void println() {}
  void println(int, int = 10) {}
  void println(const char *) {}
  void println(unsigned int, int = 10) {}
  void println(long, int = 10) {}
  void println(unsigned long, int = 10) {}
  void println(char) {}
  void print(const char *) {}
  void print(int, int = 10) {}
  void print(unsigned int, int = 10) {}
  void print(long, int = 10) {}
  void print(unsigned long, int = 10) {}
  void print(char) {}
};

inline SerialStub Serial;

inline void delay(unsigned long) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return 0; }

inline long random(long max) { return rand() % max; }
inline long random(long min, long max) { return min + rand() % (max - min); }

#define F(x) (x)
#define PROGMEM

using byte = uint8_t;

class __FlashStringHelper {};

inline uint8_t pgm_read_byte(const uint8_t *pointer) { return *pointer; }
inline void delayMicroseconds(unsigned int) {}

#endif
