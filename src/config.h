#ifndef EPD_TESTER_CONFIG_H
#define EPD_TESTER_CONFIG_H

#include <Arduino.h>

#if defined(BOARD_PROFILE_ESP32_C3)
#define PIN_EPD_MOSI 6
#define PIN_EPD_SCK 4
#define PIN_EPD_CS 7
#define PIN_EPD_DC 1
#define PIN_EPD_RST 2
#define PIN_EPD_BUSY 10
#define PIN_EPD_CS1 5
#define PIN_CFG_BTN 9
#elif defined(BOARD_PROFILE_ESP32_C3_WROOM02)
#define PIN_EPD_MOSI 6
#define PIN_EPD_SCK 4
#define PIN_EPD_CS 7
#define PIN_EPD_DC 1
#define PIN_EPD_RST 2
#define PIN_EPD_BUSY 10
#define PIN_EPD_CS1 5
#define PIN_CFG_BTN 9
#elif defined(BOARD_PROFILE_ESP32_WROOM32E) || defined(BOARD_PROFILE_SMT_WROOM32E)
#define PIN_EPD_MOSI 14
#define PIN_EPD_SCK 13
#define PIN_EPD_CS 15
#define PIN_EPD_DC 27
#define PIN_EPD_RST 26
#define PIN_EPD_BUSY 25
#define PIN_EPD_CS1 5
#define PIN_CFG_BTN 0
#elif defined(BOARD_PROFILE_SMT_C3)
#define PIN_EPD_MOSI 6
#define PIN_EPD_SCK 4
#define PIN_EPD_CS 7
#define PIN_EPD_DC 1
#define PIN_EPD_RST 2
#define PIN_EPD_BUSY 10
#define PIN_EPD_CS1 5
#define PIN_CFG_BTN 9
#elif defined(BOARD_PROFILE_YD_ESP32_S3_N16R8)
#define PIN_EPD_MOSI 11
#define PIN_EPD_SCK 12
#define PIN_EPD_CS 10
#define PIN_EPD_DC 9
#define PIN_EPD_RST 8
#define PIN_EPD_BUSY 7
#define PIN_EPD_CS1 5
#define PIN_CFG_BTN 0
#else
#error "Select a supported BOARD_PROFILE_* in platformio.ini"
#endif

#ifndef EPD_WIDTH
#define EPD_WIDTH 400
#endif
#ifndef EPD_HEIGHT
#define EPD_HEIGHT 300
#endif
#ifndef EPD_BPP
#define EPD_BPP 1
#endif
#ifndef SERIAL_BAUD_RATE
#define SERIAL_BAUD_RATE 115200
#endif
#ifndef SERIAL_BOOT_WAIT_MS
#define SERIAL_BOOT_WAIT_MS 1500
#endif

#if EPD_WIDTH <= 0 || EPD_HEIGHT <= 0
#error "EPD_WIDTH and EPD_HEIGHT must be positive"
#endif
#if EPD_BPP != 1 && EPD_BPP != 2 && EPD_BPP != 4
#error "EPD_BPP must be 1, 2, or 4"
#endif
#if EPD_BPP == 2 && ((EPD_WIDTH * EPD_HEIGHT) % 4) != 0
#error "EPD_WIDTH * EPD_HEIGHT must be divisible by 4 for 2bpp"
#endif
#if EPD_BPP == 4 && ((EPD_WIDTH * EPD_HEIGHT) % 2) != 0
#error "EPD_WIDTH * EPD_HEIGHT must be divisible by 2 for 4bpp"
#endif
#if EPD_BPP == 4 && !defined(EPD_PANEL_E6_JD7601)
#error "4bpp is currently supported only by EPD_PANEL_E6_JD7601"
#endif

static constexpr int W = EPD_WIDTH;
static constexpr int H = EPD_HEIGHT;
static constexpr int ROW_BYTES = (W + 7) / 8;
static constexpr int IMG_BUF_LEN = ROW_BYTES * H;
static constexpr int COLOR_BUF_LEN = (W * H * EPD_BPP) / 8;

#if defined(BOARD_PROFILE_ESP32_C3)
#define BOARD_PROFILE_LABEL "ESP32-C3"
#elif defined(BOARD_PROFILE_ESP32_C3_WROOM02)
#define BOARD_PROFILE_LABEL "ESP32-C3-WROOM02"
#elif defined(BOARD_PROFILE_ESP32_WROOM32E)
#define BOARD_PROFILE_LABEL "ESP32-WROOM32E"
#elif defined(BOARD_PROFILE_SMT_WROOM32E)
#define BOARD_PROFILE_LABEL "SMT-WROOM32E"
#elif defined(BOARD_PROFILE_SMT_C3)
#define BOARD_PROFILE_LABEL "SMT-C3"
#elif defined(BOARD_PROFILE_YD_ESP32_S3_N16R8)
#define BOARD_PROFILE_LABEL "YD-ESP32-S3-N16R8"
#endif

#if defined(EPD_PANEL_42_SSD1683_BW)
#define EPD_PANEL_LABEL "SSD1683 software SPI"
#elif defined(EPD_PANEL_E6_JD7601)
#define EPD_PANEL_LABEL "GDEH037E01/JD7601 E6"
#elif defined(EPD_PANEL_42_SSD1619A_BW)
#define EPD_PANEL_LABEL "SSD1619A"
#elif defined(EPD_PANEL_42_SSD1608_DUAL_BW)
#define EPD_PANEL_LABEL "dual SSD1608"
#elif defined(EPD_PANEL_42_GDEM042F52)
#define EPD_PANEL_LABEL "GDEM042F52/JD79668"
#elif defined(EPD_PANEL_42_DKE_RY683)
#define EPD_PANEL_LABEL "DEPG0420/RY683"
#elif defined(EPD_PANEL_42_WFT)
#define EPD_PANEL_LABEL "WFT042-compatible"
#elif defined(EPD_PANEL_42_GXEPD2_GYE042A87)
#define EPD_PANEL_LABEL "GYE042A87/GxEPD2"
#elif defined(EPD_PANEL_42_GXEPD2_T81)
#define EPD_PANEL_LABEL "GDEY042T81/GxEPD2"
#elif defined(EPD_PANEL_42_GXEPD2_420)
#define EPD_PANEL_LABEL "GDEW042T2/IL0398"
#elif defined(EPD_PANEL_42_GXEPD2_M01)
#define EPD_PANEL_LABEL "GDEW042M01/UC8176"
#elif defined(EPD_PANEL_42_BLOZIENDOR_BWR)
#define EPD_PANEL_LABEL "KEGM042001T06/P420016"
#elif defined(EPD_PANEL_42_HINK_E042A31_BWR)
#define EPD_PANEL_LABEL "HINK-E042A31"
#elif defined(EPD_PANEL_213_HINK_E0213A127_BWR)
#define EPD_PANEL_LABEL "HINK-E0213A127/SSD1680"
#elif defined(EPD_PANEL_213_A1360069_IL0373_BWR)
#define EPD_PANEL_LABEL "A1360069/IL0373"
#elif defined(EPD_PANEL_29_SSD1608_V1)
#define EPD_PANEL_LABEL "2.9-inch SSD1608 V1"
#elif defined(EPD_PANEL_29)
#define EPD_PANEL_LABEL "GDEY029T94"
#elif defined(EPD_PANEL_583_UC8179) || defined(EPD_PANEL_583)
#define EPD_PANEL_LABEL "GDEQ0583T31/UC8179"
#elif defined(EPD_PANEL_75)
#define EPD_PANEL_LABEL "GDEY075T7"
#else
#define EPD_PANEL_LABEL "unknown panel"
#endif

extern uint8_t imgBuf[IMG_BUF_LEN];
#if EPD_BPP >= 2
extern uint8_t *colorBuf;
extern bool useColorBuf;
bool ensureColorBuf();
#endif

#endif
