#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "epd_driver.h"

#include <stdlib.h>

uint8_t imgBuf[IMG_BUF_LEN];

#if EPD_BPP >= 2
#if EPD_BPP == 4
// Keep the fixed E6 frame in BSS instead of requesting a large heap block at startup.
static uint8_t e6ColorBuf[COLOR_BUF_LEN];
uint8_t *colorBuf = e6ColorBuf;
#else
uint8_t *colorBuf = nullptr;
#endif
bool useColorBuf = false;

bool ensureColorBuf() {
#if EPD_BPP == 4
    return colorBuf == e6ColorBuf;
#else
    if (!colorBuf) {
        colorBuf = static_cast<uint8_t *>(malloc(COLOR_BUF_LEN));
    }
    return colorBuf != nullptr;
#endif
}
#endif

namespace {

constexpr unsigned long kSlideIntervalMs = 10000;
unsigned long lastDisplayAt = 0;
uint8_t slideIndex = 0;

void displayCurrentSlide() {
    const unsigned long renderStartedAt = millis();
    Serial.printf("[DISPLAY] slide %u/%u: render start (%dx%d, %d bpp)\n",
                  static_cast<unsigned>(slideIndex + 1),
                  static_cast<unsigned>(TEST_SLIDE_COUNT), W, H, EPD_BPP);
    renderTestSlide(slideIndex);
    Serial.printf("[DISPLAY] framebuffer ready (%u bytes, render %lu ms)\n",
#if EPD_BPP >= 2
                  static_cast<unsigned>(COLOR_BUF_LEN),
#else
                  static_cast<unsigned>(IMG_BUF_LEN),
#endif
                  millis() - renderStartedAt);
    Serial.println("[DISPLAY] full-screen transfer and refresh start");
    const unsigned long refreshStartedAt = millis();
#if EPD_BPP == 4
    epdDisplay4bpp(colorBuf);
#elif EPD_BPP == 2
    epdDisplay2bpp(colorBuf);
#else
    epdDisplay(imgBuf);
#endif
    lastDisplayAt = millis();
    if (slideIndex + 1 >= TEST_SLIDE_COUNT) {
        Serial.printf("[DISPLAY] one carousel complete after %lu ms; holding final slide\n",
                      lastDisplayAt - refreshStartedAt);
    } else {
        Serial.printf("[DISPLAY] driver returned after %lu ms; next slide in %lu ms\n",
                      lastDisplayAt - refreshStartedAt, kSlideIntervalMs);
    }
}

} // namespace

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    const unsigned long serialWaitStartedAt = millis();
#if ARDUINO_USB_CDC_ON_BOOT
    while (!Serial && millis() - serialWaitStartedAt < SERIAL_BOOT_WAIT_MS) {
        delay(10);
    }
#else
    delay(50);
#endif
    Serial.printf("\n[BOOT] serial ready at %u baud (wait %lu ms)\n",
                  SERIAL_BAUD_RATE, millis() - serialWaitStartedAt);
    Serial.printf("[BOOT] board=%s panel=%s resolution=%dx%d bpp=%d\n",
                  BOARD_PROFILE_LABEL, EPD_PANEL_LABEL, W, H, EPD_BPP);
    Serial.printf("[BOOT] EPD pins MOSI=%d SCK=%d CS=%d DC=%d RST=%d BUSY=%d\n",
                  PIN_EPD_MOSI, PIN_EPD_SCK, PIN_EPD_CS, PIN_EPD_DC,
                  PIN_EPD_RST, PIN_EPD_BUSY);

#if EPD_BPP >= 2
    Serial.printf("[BOOT] preparing %d-bpp framebuffer (%u bytes)\n",
                  EPD_BPP,
                  static_cast<unsigned>(COLOR_BUF_LEN));
    if (!ensureColorBuf()) {
        Serial.println("[ERROR] color framebuffer is unavailable");
        while (true) {
            delay(1000);
        }
    }
    Serial.println("[BOOT] 2bpp framebuffer ready");
#endif

    Serial.println("[BOOT] GPIO initialization start");
    gpioInit();
    Serial.println("[BOOT] GPIO initialization complete");
    Serial.println("[BOOT] EPD controller initialization start");
    const unsigned long epdInitStartedAt = millis();
    epdInit();
    Serial.printf("[BOOT] EPD controller initialization call returned after %lu ms\n",
                  millis() - epdInitStartedAt);
    displayCurrentSlide();
}

void loop() {
    if (slideIndex + 1 >= TEST_SLIDE_COUNT) {
        delay(1000);
        return;
    }
    if (millis() - lastDisplayAt >= kSlideIntervalMs) {
        ++slideIndex;
        displayCurrentSlide();
    }
    delay(20);
}
