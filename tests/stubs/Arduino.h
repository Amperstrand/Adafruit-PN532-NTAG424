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
  void println(int, int = 10) {}
  void println(const char *) {}
  void print(const char *) {}
  void print(int, int = 10) {}
};

extern SerialStub Serial;

inline void delay(unsigned long) {}

inline long random(long max) { return rand() % max; }
inline long random(long min, long max) { return min + rand() % (max - min); }

#define F(x) (x)
#define PROGMEM

#endif
