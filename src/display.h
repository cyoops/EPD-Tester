#ifndef EPD_TESTER_DISPLAY_H
#define EPD_TESTER_DISPLAY_H

#include <Arduino.h>
#include "config.h"

#if EPD_BPP == 4
constexpr uint8_t TEST_SLIDE_COUNT = 8;
#else
constexpr uint8_t TEST_SLIDE_COUNT = 6;
#endif

void renderTestSlide(uint8_t slide);

#endif
