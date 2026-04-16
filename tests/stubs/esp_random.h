// Desktop stub for esp_random.h
#ifndef ESP_RANDOM_H
#define ESP_RANDOM_H

#include <stdint.h>
#include <cstdlib>

static inline uint32_t esp_random() { return (uint32_t)std::rand(); }

#endif
