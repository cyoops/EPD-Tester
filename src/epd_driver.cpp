#include "epd_driver.h"
#include "config.h"

#include <string.h>

#if defined(EPD_PANEL_42_SSD1619A_BW)

namespace {

constexpr unsigned long kShortBusyTimeoutMs = 250;
constexpr unsigned long kRefreshBusyTimeoutMs = 10000;
constexpr uint8_t kBusyActiveLevel = HIGH;

const uint8_t kFullRefreshLut[] = {
    0x08, 0x00, 0x48, 0x40, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x12, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x20, 0x20, 0x05, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x40, 0x20, 0x20, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
};

static_assert(sizeof(kFullRefreshLut) == 70, "SSD1619A full LUT must be 70 bytes");

bool controllerInitialized = false;

bool waitUntilReady(unsigned long timeoutMs) {
    const unsigned long start = millis();
    while (digitalRead(PIN_EPD_BUSY) == kBusyActiveLevel) {
        delay(2);
        if (millis() - start >= timeoutMs) {
            Serial.printf("[EPD-SSD1619A] BUSY remained high for %lu ms\n", timeoutMs);
            return false;
        }
    }
    return true;
}

void writeByte(uint8_t value) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        digitalWrite(PIN_EPD_MOSI, (value & 0x80) ? HIGH : LOW);
        value <<= 1;
        digitalWrite(PIN_EPD_SCK, HIGH);
        digitalWrite(PIN_EPD_SCK, LOW);
    }
}

void writeCommand(uint8_t command) {
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, LOW);
    writeByte(command);
    digitalWrite(PIN_EPD_CS, HIGH);
}

void writeData(const uint8_t *data, size_t length) {
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    for (size_t i = 0; i < length; ++i) {
        writeByte(data[i]);
    }
    digitalWrite(PIN_EPD_CS, HIGH);
}

void writeCommandData(uint8_t command, const uint8_t *data, size_t length) {
    writeCommand(command);
    writeData(data, length);
}

bool resetController() {
    digitalWrite(PIN_EPD_RST, LOW);
    delay(10);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(10);
    return waitUntilReady(kShortBusyTimeoutMs);
}

void setRamWindow(int x, int y, int width, int height) {
    const uint8_t xStart = static_cast<uint8_t>(x / 8);
    const uint8_t xEnd = static_cast<uint8_t>((x + width - 1) / 8);
    const uint16_t yEnd = static_cast<uint16_t>(y + height - 1);
    const uint8_t xRange[] = {xStart, xEnd};
    const uint8_t yRange[] = {
        static_cast<uint8_t>(y),
        static_cast<uint8_t>(y >> 8),
        static_cast<uint8_t>(yEnd),
        static_cast<uint8_t>(yEnd >> 8),
    };
    const uint8_t xCounter[] = {xStart};
    const uint8_t yCounter[] = {
        static_cast<uint8_t>(y),
        static_cast<uint8_t>(y >> 8),
    };
    writeCommandData(0x44, xRange, sizeof(xRange));
    writeCommandData(0x45, yRange, sizeof(yRange));
    writeCommandData(0x4E, xCounter, sizeof(xCounter));
    writeCommandData(0x4F, yCounter, sizeof(yCounter));
}

bool writeRam(uint8_t command, const uint8_t *data) {
    if (!waitUntilReady(kShortBusyTimeoutMs)) return false;
    setRamWindow(0, 0, W, H);
    writeCommand(command);
    writeData(data, IMG_BUF_LEN);
    return true;
}

bool refresh() {
    const unsigned long start = millis();
    writeCommand(0x20);
    if (!waitUntilReady(kRefreshBusyTimeoutMs)) return false;
    Serial.printf("[EPD-SSD1619A] refresh complete in %lu ms, BUSY=%d\n",
                  millis() - start, digitalRead(PIN_EPD_BUSY));
    return true;
}

bool initializeController() {
    if (!resetController()) return false;

    const uint8_t analogBlock[] = {0x54};
    const uint8_t digitalBlock[] = {0x3B};
    const uint8_t panelSetting[] = {0x2B, 0x01, 0x00};
    const uint8_t boosterSetting[] = {0x8B, 0x9C, 0xD6, 0x0F};
    const uint8_t dummyLinePeriod[] = {0x21};
    const uint8_t gateLineWidth[] = {0x06};
    const uint8_t borderSetting[] = {0x03};
    const uint8_t dataEntryMode[] = {0x03};
    const uint8_t vcomSetting[] = {0x00};
    const uint8_t ramOption[] = {0x00, 0x00, 0x00, 0x00, 0x80};
    const uint8_t updateControl1[] = {0x40};
    const uint8_t updateControl2[] = {0xC7};
    writeCommandData(0x74, analogBlock, sizeof(analogBlock));
    writeCommandData(0x7E, digitalBlock, sizeof(digitalBlock));
    writeCommandData(0x01, panelSetting, sizeof(panelSetting));
    writeCommandData(0x0C, boosterSetting, sizeof(boosterSetting));
    writeCommandData(0x3A, dummyLinePeriod, sizeof(dummyLinePeriod));
    writeCommandData(0x3B, gateLineWidth, sizeof(gateLineWidth));
    writeCommandData(0x3C, borderSetting, sizeof(borderSetting));
    writeCommandData(0x2C, vcomSetting, sizeof(vcomSetting));
    writeCommandData(0x37, ramOption, sizeof(ramOption));
    writeCommandData(0x11, dataEntryMode, sizeof(dataEntryMode));
    writeCommandData(0x21, updateControl1, sizeof(updateControl1));
    writeCommandData(0x22, updateControl2, sizeof(updateControl2));
    writeCommandData(0x32, kFullRefreshLut, sizeof(kFullRefreshLut));
    if (!waitUntilReady(kShortBusyTimeoutMs)) return false;
    controllerInitialized = true;
    return true;
}

bool displayFrame(const uint8_t *image) {
    if (!controllerInitialized && !initializeController()) {
        controllerInitialized = false;
        Serial.println("[EPD-SSD1619A] controller initialization failed");
        return false;
    }

    if (!writeRam(0x24, image)) {
        Serial.println("[EPD-SSD1619A] BUSY did not clear before RAM write");
        return false;
    }
    return refresh();
}

} // namespace

void gpioInit() {
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_RST, OUTPUT);
    pinMode(PIN_EPD_DC, OUTPUT);
    pinMode(PIN_EPD_CS, OUTPUT);
    pinMode(PIN_EPD_SCK, OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    digitalWrite(PIN_EPD_RST, HIGH);
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);
}

void epdInit() {
    if (!initializeController()) {
        controllerInitialized = false;
        Serial.println("[EPD-SSD1619A] initialization failed");
    }
}

void epdInitFast() {
    epdInit();
}

void epdDisplay(const uint8_t *image) {
    if (image && !displayFrame(image)) {
        Serial.printf("[EPD-SSD1619A] display failed, BUSY=%d\n",
                      digitalRead(PIN_EPD_BUSY));
    }
}

void epdDisplayFast(const uint8_t *image) {
    epdDisplay(image);
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
    (void)image2bpp;
    epdDisplay(imgBuf);
}

void epdDisplayDeepClear(const uint8_t *image) {
    epdDisplay(image);
}

bool epdSupportsPartialRefresh() {
    return false;
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    (void)data;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    epdDisplay(imgBuf);
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData,
                              int xStart, int yStart, int xEnd, int yEnd) {
    (void)oldData;
    epdPartialDisplay(data, xStart, yStart, xEnd, yEnd);
}

void epdSleep() {
    if (!controllerInitialized) return;
    if (!waitUntilReady(kRefreshBusyTimeoutMs)) return;
    const uint8_t sleepMode[] = {0x01};
    writeCommandData(0x10, sleepMode, sizeof(sleepMode));
    delay(10);
    controllerInitialized = false;
}

#elif defined(EPD_PANEL_42_SSD1608_DUAL_BW)

#if !defined(PIN_EPD_CS1)
#error "EPD_PANEL_42_SSD1608_DUAL_BW requires PIN_EPD_CS1"
#endif

#if EPD_WIDTH != 400 || EPD_HEIGHT != 300
#error "The dual SSD1608 driver requires a 400x300 panel"
#endif

namespace {

constexpr size_t kSSD1608HalfWidthBytes = EPD_WIDTH / 16;
constexpr uint32_t kSSD1608BusyTimeoutMs = 60000;
bool ssd1608Initialized = false;

// Adafruit's 30-byte SSD1608 waveform table, used when the panel OTP LUT is blank.
const uint8_t kSSD1608Lut[] = {
    0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
    0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
    0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00,
};

static_assert(sizeof(kSSD1608Lut) == 30, "SSD1608 LUT must be 30 bytes");

void ssd1608WriteByte(uint8_t value) {
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        digitalWrite(PIN_EPD_SCK, LOW);
        digitalWrite(PIN_EPD_MOSI, (value & mask) ? HIGH : LOW);
        digitalWrite(PIN_EPD_SCK, HIGH);
    }
    digitalWrite(PIN_EPD_SCK, LOW);
}

void ssd1608WriteCommand(uint8_t command, const uint8_t *data, size_t dataLength,
                         bool selectCs0, bool selectCs1) {
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, selectCs0 ? LOW : HIGH);
    digitalWrite(PIN_EPD_CS1, selectCs1 ? LOW : HIGH);
    ssd1608WriteByte(command);

    if (dataLength != 0) {
        digitalWrite(PIN_EPD_DC, HIGH);
        for (size_t i = 0; i < dataLength; i++) {
            ssd1608WriteByte(data[i]);
        }
    }

    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_CS1, HIGH);
}

void ssd1608WriteBoth(uint8_t command, const uint8_t *data = nullptr, size_t dataLength = 0) {
    ssd1608WriteCommand(command, data, dataLength, true, true);
}

void ssd1608WriteOne(uint8_t command, const uint8_t *data, size_t dataLength, bool cs1) {
    ssd1608WriteCommand(command, data, dataLength, !cs1, cs1);
}

bool ssd1608WaitUntilReady(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (digitalRead(PIN_EPD_BUSY) == HIGH) {
        if (millis() - start >= timeoutMs) {
            Serial.println("[EPD-SSD1608x2] BUSY timeout");
            return false;
        }
        delay(10);
    }
    return true;
}

void ssd1608Reset() {
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_CS1, HIGH);
    digitalWrite(PIN_EPD_RST, LOW);
    delay(10);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(10);
    ssd1608WriteBoth(0x12);
}

bool ssd1608Initialize() {
    ssd1608Initialized = false;
    ssd1608Reset();

    const uint8_t panelSetting[] = {
        static_cast<uint8_t>((EPD_HEIGHT - 1) & 0xFF),
        static_cast<uint8_t>((EPD_HEIGHT - 1) >> 8),
        0x00,
    };
    const uint8_t gateDrivingVoltage[] = {0x1B};
    const uint8_t sourceDrivingVoltage[] = {0x0B};
    const uint8_t dummyLinePeriod[] = {0x33};
    const uint8_t dataEntryMode[] = {0x03};
    const uint8_t vcomVoltage[] = {0x70};

    ssd1608WriteBoth(0x01, panelSetting, sizeof(panelSetting));
    ssd1608WriteBoth(0x3A, gateDrivingVoltage, sizeof(gateDrivingVoltage));
    ssd1608WriteBoth(0x3B, sourceDrivingVoltage, sizeof(sourceDrivingVoltage));
    ssd1608WriteBoth(0x3C, dummyLinePeriod, sizeof(dummyLinePeriod));
    ssd1608WriteBoth(0x11, dataEntryMode, sizeof(dataEntryMode));
    ssd1608WriteBoth(0x2C, vcomVoltage, sizeof(vcomVoltage));
    ssd1608WriteBoth(0x32, kSSD1608Lut, sizeof(kSSD1608Lut));

    ssd1608Initialized = true;
    return true;
}

void ssd1608WriteHalf(const uint8_t *image, bool cs1) {
    const uint8_t xWindow[] = {0x00, static_cast<uint8_t>(kSSD1608HalfWidthBytes - 1)};
    const uint8_t xCounter[] = {0x00};
    const uint8_t yWindow[] = {
        0x00,
        0x00,
        static_cast<uint8_t>((EPD_HEIGHT - 1) & 0xFF),
        static_cast<uint8_t>((EPD_HEIGHT - 1) >> 8),
    };
    const uint8_t yCounter[] = {0x00, 0x00};

    ssd1608WriteOne(0x44, xWindow, sizeof(xWindow), cs1);
    ssd1608WriteOne(0x4E, xCounter, sizeof(xCounter), cs1);
    ssd1608WriteOne(0x45, yWindow, sizeof(yWindow), cs1);
    ssd1608WriteOne(0x4F, yCounter, sizeof(yCounter), cs1);
    ssd1608WriteOne(0x24, nullptr, 0, cs1);

    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(cs1 ? PIN_EPD_CS1 : PIN_EPD_CS, LOW);
    const size_t rowBytes = EPD_WIDTH / 8;
    const size_t halfOffset = cs1 ? kSSD1608HalfWidthBytes : 0;
    for (size_t y = 0; y < EPD_HEIGHT; y++) {
        const uint8_t *row = image + y * rowBytes + halfOffset;
        for (size_t x = 0; x < kSSD1608HalfWidthBytes; x++) {
            ssd1608WriteByte(row[x]);
        }
    }
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_CS1, HIGH);
}

bool ssd1608Refresh() {
    const uint8_t updateControl[] = {0xC7};
    ssd1608WriteBoth(0x22, updateControl, sizeof(updateControl));
    ssd1608WriteOne(0x20, nullptr, 0, true);
    ssd1608WriteOne(0x20, nullptr, 0, false);
    delay(10);
    return ssd1608WaitUntilReady(kSSD1608BusyTimeoutMs);
}

void ssd1608Display(const uint8_t *image) {
    if (image == nullptr) return;
    if (!ssd1608Initialized && !ssd1608Initialize()) {
        Serial.println("[EPD-SSD1608x2] initialization failed");
        return;
    }
    ssd1608WriteHalf(image, false);
    ssd1608WriteHalf(image, true);
    if (!ssd1608Refresh()) {
        ssd1608Initialized = false;
        Serial.printf("[EPD-SSD1608x2] display failed, BUSY=%d\n", digitalRead(PIN_EPD_BUSY));
    }
}

}  // namespace

void gpioInit() {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    pinMode(PIN_EPD_CS, OUTPUT);
    pinMode(PIN_EPD_CS1, OUTPUT);
    pinMode(PIN_EPD_DC, OUTPUT);
    pinMode(PIN_EPD_RST, OUTPUT);
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_SCK, OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_CS1, HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);
    digitalWrite(PIN_EPD_DC, LOW);
}

void epdInit() {
    if (!ssd1608Initialize()) {
        Serial.println("[EPD-SSD1608x2] initialization failed");
    }
}

void epdInitFast() {
    epdInit();
}

void epdDisplay(const uint8_t *image) {
    ssd1608Display(image);
}

void epdDisplayFast(const uint8_t *image) {
    epdDisplay(image);
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
    (void)image2bpp;
    epdDisplay(imgBuf);
}

void epdDisplayDeepClear(const uint8_t *image) {
    epdDisplay(image);
}

bool epdSupportsPartialRefresh() {
    return false;
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    (void)data;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    epdDisplay(imgBuf);
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData,
                              int xStart, int yStart, int xEnd, int yEnd) {
    (void)oldData;
    epdPartialDisplay(data, xStart, yStart, xEnd, yEnd);
}

void epdSleep() {
    if (!ssd1608Initialized) return;
    const uint8_t sleepMode[] = {0x01};
    ssd1608WriteBoth(0x10, sleepMode, sizeof(sleepMode));
    delay(10);
    ssd1608Initialized = false;
}

#else

// The generic SSD1683 software-SPI profile is tested with Waveshare 4.2" V2
// and Zhongjingyuan 4.2" BW modules (400x300). GDEY042T81 also uses SSD1683,
// but its GxEPD2 driver path is intentionally separate below.
#if defined(EPD_PANEL_42_SSD1683_BW) || defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)

// ── Software SPI (bit-bang) for 4.2" panels ──
// Avoids Busy Timeout on ESP32-C3 with non-default pins; no GxEPD2 dependency.

static void spiWriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_EPD_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        digitalWrite(PIN_EPD_SCK, HIGH);
        digitalWrite(PIN_EPD_SCK, LOW);
    }
}

static void epdSendCommand(uint8_t cmd) {
    digitalWrite(PIN_EPD_DC, LOW);   // DC low = command
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(cmd);
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epdSendData(uint8_t data) {
    digitalWrite(PIN_EPD_DC, HIGH);  // DC high = data
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(data);
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epdWaitBusy(unsigned long maxMs = 0) {
    unsigned long t0 = millis();
    unsigned long timeoutMs = maxMs > 0 ? maxMs :
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
        45000;
#else
        10000;
#endif
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    while (digitalRead(PIN_EPD_BUSY) == LOW) {
#else
    while (digitalRead(PIN_EPD_BUSY) == HIGH) {
#endif
        delay(10);
        if (millis() - t0 > timeoutMs) {
            Serial.println("EPD busy TIMEOUT!");
            return;
        }
    }
#if defined(EPD_PANEL_42_DKE_RY683)
    delay(100);
#endif
}

static void epdReset() {
#if defined(EPD_PANEL_42_GDEM042F52)
    delay(20);
    digitalWrite(PIN_EPD_RST, LOW);  delay(40);
    digitalWrite(PIN_EPD_RST, HIGH); delay(50);
#elif defined(EPD_PANEL_42_DKE_RY683)
    digitalWrite(PIN_EPD_RST, LOW);  delay(10);
    digitalWrite(PIN_EPD_RST, HIGH); delay(10);
#else
    digitalWrite(PIN_EPD_RST, HIGH); delay(100);
    digitalWrite(PIN_EPD_RST, LOW);  delay(2);
    digitalWrite(PIN_EPD_RST, HIGH); delay(100);
#endif
}

// ── Helper: configure RAM window for full screen ────────────

static void epdSetFullWindow() {
    epdSendCommand(0x11);  // Data Entry Mode Setting
    epdSendData(0x03);     //   X increment, Y increment

    epdSendCommand(0x44);  // Set RAM X address range
    epdSendData(0x00);
    epdSendData((W - 1) / 8);

    epdSendCommand(0x45);  // Set RAM Y address range
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData((H - 1) & 0xFF);
    epdSendData(((H - 1) >> 8) & 0xFF);

    epdSendCommand(0x4E);  // Set RAM X address counter
    epdSendData(0x00);

    epdSendCommand(0x4F);  // Set RAM Y address counter
    epdSendData(0x00);
    epdSendData(0x00);
}

// ── GPIO initialization ─────────────────────────────────────

void gpioInit() {
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_RST,  OUTPUT);
    pinMode(PIN_EPD_DC,   OUTPUT);
    pinMode(PIN_EPD_CS,   OUTPUT);
    pinMode(PIN_EPD_SCK,  OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);
    pinMode(PIN_CFG_BTN,  INPUT_PULLUP);
    digitalWrite(PIN_EPD_RST, HIGH);
    digitalWrite(PIN_EPD_CS,  HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);
}

// ── EPD full init (standard mode) ──

void epdInit() {
    const unsigned long initStartedAt = millis();
    Serial.printf("[EPD-SW-SPI] controller init start (%dx%d)\n", W, H);
#if defined(EPD_PANEL_42_DKE_RY683)
    Serial.printf("[EPD-init] begin BUSY=%d\n", digitalRead(PIN_EPD_BUSY));
    epdReset();
    epdWaitBusy();
    Serial.printf("[EPD-init] wait1 done BUSY=%d\n", digitalRead(PIN_EPD_BUSY));

    epdSendCommand(0x06);
    epdSendData(0x0F);
    epdSendData(0x8B);
    epdSendData(0x9C);
    epdSendData(0x96);

    epdSendCommand(0x00);
    epdSendData(0x2F);
    epdSendData(0x69);
    epdWaitBusy();
    Serial.printf("[EPD-init] wait2 done BUSY=%d\n", digitalRead(PIN_EPD_BUSY));

    epdSendCommand(0x01);
    epdSendData(0x07);
    epdSendData(0xF0);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(0x01);
    epdSendData(0x90);
    epdSendData(0x01);
    epdSendData(0x2C);

    epdSendCommand(0x62);
    epdSendData(0x64);
    epdSendData(0x53);

    epdSendCommand(0x65);
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0xE9);
    epdSendData(0x01);
#elif defined(EPD_PANEL_42_GDEM042F52)
    epdReset();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00);
    epdSendData(0x0F);
    epdSendData(0x29);

    epdSendCommand(0x06);
    epdSendData(0x0D);
    epdSendData(0x12);
    epdSendData(0x24);
    epdSendData(0x25);
    epdSendData(0x12);
    epdSendData(0x29);
    epdSendData(0x10);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0xAE);
    epdSendData(0xCF);

    epdSendCommand(0xB0);
    epdSendData(0x13);

    epdSendCommand(0xBD);
    epdSendData(0x07);

    epdSendCommand(0xBE);
    epdSendData(0xFE);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x04);
    epdWaitBusy();
#else
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x12);  // Software Reset
    epdWaitBusy();

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x40);     //   Source output mode
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x05);

    epdSetFullWindow();
    epdWaitBusy();
#endif
    Serial.printf("[EPD] controller init sequence returned after %lu ms\n",
                  millis() - initStartedAt);
}

// ── EPD fast init (loads fast-refresh LUT via temperature register) ──
// Based on official Waveshare epd4in2_V2 Init_Fast() implementation.
// The 0x1A register sets a temperature value that selects faster internal LUT.
// 0x6E = ~1.5s refresh, 0x5A = ~1s refresh.

void epdInitFast() {
#if defined(EPD_PANEL_42_DKE_RY683)
    epdInit();
#elif defined(EPD_PANEL_42_GDEM042F52)
    delay(100);
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00);
    epdSendData(0x0F);
    epdSendData(0x29);

    epdSendCommand(0x01);
    epdSendData(0x07);
    epdSendData(0x00);

    epdSendCommand(0x03);
    epdSendData(0x10);
    epdSendData(0x54);
    epdSendData(0x44);

    epdSendCommand(0x06);
    epdSendData(0x0F);
    epdSendData(0x0A);
    epdSendData(0x2F);
    epdSendData(0x25);
    epdSendData(0x22);
    epdSendData(0x2E);
    epdSendData(0x21);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0xE3);
    epdSendData(0x22);

    epdSendCommand(0xB6);
    epdSendData(0x6F);

    epdSendCommand(0xB4);
    epdSendData(0xD0);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0x04);
    epdWaitBusy();

    epdSendCommand(0xE0);
    epdSendData(0x02);

    epdSendCommand(0xE6);
    epdSendData(0x5A);

    epdSendCommand(0xA5);
    epdSendData(0x00);
    epdWaitBusy();
#else
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x12);  // Software Reset
    epdWaitBusy();

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x40);
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x05);

    epdSendCommand(0x1A);  // Write to temperature register
    epdSendData(0x6E);     //   Value for ~1.5s fast refresh

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0x91);     //   Load temperature + Load LUT, then power down
    epdSendCommand(0x20);  // Master Activation
    epdWaitBusy();

    epdSetFullWindow();
    epdWaitBusy();
#endif
}

// ── EPD full-screen display (standard full refresh, 0xF7) ───
// Clears all ghosting but has visible black-white flash (~3-4s).

static uint8_t packMonoPixelGroup(const uint8_t *image, int rowBytes, int y, int x) {
    uint8_t packed = 0;
    for (int bit = 0; bit < 4; bit++) {
        int px = x + bit;
        bool isBlack = (image[y * rowBytes + px / 8] & (0x80 >> (px % 8))) == 0;
        uint8_t color = isBlack ? 0x00 : 0x01;
        packed |= color << (6 - bit * 2);
    }
    return packed;
}

#if defined(EPD_PANEL_42_GDEM042F52)
static uint8_t epdRemap2bppColor(uint8_t color) {
    return color & 0x03;
}

static void epdWriteMapped2bpp(const uint8_t *buf2bpp) {
    epdSendCommand(0x10);
    for (int i = 0; i < COLOR_BUF_LEN; i++) {
        uint8_t src = buf2bpp[i];
        uint8_t dst = 0;
        dst |= epdRemap2bppColor((src >> 6) & 0x03) << 6;
        dst |= epdRemap2bppColor((src >> 4) & 0x03) << 4;
        dst |= epdRemap2bppColor((src >> 2) & 0x03) << 2;
        dst |= epdRemap2bppColor(src & 0x03);
        epdSendData(dst);
    }
}

static void epdPowerOff() {
    epdSendCommand(0x02);
    epdSendData(0x00);
    epdWaitBusy();
}
#endif

static void epdSend2bppAndRefresh(const uint8_t *buf2bpp) {
    for (int attempt = 0; attempt < 3; attempt++) {
        unsigned long t0 = millis();
        Serial.printf("[EPD] attempt %d start BUSY=%d\n", attempt, digitalRead(PIN_EPD_BUSY));
        epdInit();
        Serial.printf("[EPD] init done %lums BUSY=%d\n", millis()-t0, digitalRead(PIN_EPD_BUSY));
#if defined(EPD_PANEL_42_GDEM042F52)
        epdWriteMapped2bpp(buf2bpp);
#else
        epdSendCommand(0x10);
        for (int i = 0; i < COLOR_BUF_LEN; i++) {
            epdSendData(buf2bpp[i]);
        }
#endif
        Serial.printf("[EPD] data done %lums\n", millis()-t0);
#if defined(EPD_PANEL_42_GDEM042F52)
        epdSendCommand(0x12);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] refresh done %lums\n", millis()-t0);
        epdPowerOff();
        Serial.printf("[EPD] all done %lums\n", millis()-t0);
        return;
#else
        epdSendCommand(0x04);
        delay(100);
        if (digitalRead(PIN_EPD_BUSY) == HIGH) {
            Serial.printf("[EPD] cmd04 no response, retry\n");
            continue;
        }
        epdWaitBusy();
        Serial.printf("[EPD] power-on done %lums\n", millis()-t0);
        epdSendCommand(0x12);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] refresh done %lums\n", millis()-t0);
        epdSendCommand(0x02);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] all done %lums\n", millis()-t0);
        return;
#endif
    }
    Serial.println("[EPD] display failed after 3 attempts");
}

void epdDisplay(const uint8_t *image) {
    if (!image) {
        Serial.println("[EPD] full-screen refresh aborted: null image");
        return;
    }
    const unsigned long refreshStartedAt = millis();
    Serial.printf("[EPD] full-screen refresh start, %d bytes\n", IMG_BUF_LEN);
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    if (!ensureColorBuf()) { Serial.println("[EPD] colorBuf alloc failed"); return; }
    int rowBytes = W / 8;
    int out = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x += 4) {
            colorBuf[out++] = packMonoPixelGroup(image, rowBytes, y, x);
        }
    }
    epdSend2bppAndRefresh(colorBuf);
#else
    epdInit();

    int w = W / 8;

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);  // Write RED RAM (old data for refresh)
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xF7);     //   Full update sequence
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
    Serial.printf("[EPD] full-screen refresh sequence returned after %lu ms\n",
                  millis() - refreshStartedAt);
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdSend2bppAndRefresh(image2bpp);
#else
    (void)image2bpp;
    epdDisplay(imgBuf);
#endif
}

// ── EPD deep clear (multi-cycle anti-ghosting) ──────────────
// SSD1683 BW: cycles all-black/all-white via 0x24/0x26 registers.
// 4-color panels: falls back to epdDisplay (no register-level deep clear).

void epdDisplayDeepClear(const uint8_t *image) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdDisplay(image);
#else
    epdInit();
    int w = W / 8;
    int total = w * H;

    for (int pass = 0; pass < 4; pass++) {
        uint8_t fill = (pass % 2 == 0) ? 0x00 : 0xFF;

        epdSendCommand(0x24);
        for (int i = 0; i < total; i++)
            epdSendData(fill);

        epdSendCommand(0x26);
        for (int i = 0; i < total; i++)
            epdSendData(fill);

        epdSendCommand(0x22);
        epdSendData(0xF7);
        epdSendCommand(0x20);
        epdWaitBusy();
    }

    epdSendCommand(0x24);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);
    epdSendData(0xF7);
    epdSendCommand(0x20);
    epdWaitBusy();
#endif
}

// ── EPD full-screen display (fast refresh, 0xC7) ────────────

void epdDisplayFast(const uint8_t *image) {
#if defined(EPD_PANEL_42_GDEM042F52)
    if (!ensureColorBuf()) { Serial.println("[EPD] colorBuf alloc failed"); return; }
    int rowBytes = W / 8;
    int out = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x += 4) {
            colorBuf[out++] = packMonoPixelGroup(image, rowBytes, y, x);
        }
    }
    epdInitFast();
    epdWriteMapped2bpp(colorBuf);
    epdSendCommand(0x12);
    epdSendData(0x00);
    epdWaitBusy();
    epdPowerOff();
#elif defined(EPD_PANEL_42_DKE_RY683)
    epdDisplay(image);
#else
    epdInitFast();

    int w = W / 8;

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);  // Write RED RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xC7);     //   Fast update: skip LUT load (already loaded by InitFast)
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
}

// ── EPD partial refresh ─────────────────────────────────────

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    epdPartialDisplayWithOld(data, nullptr, xStart, yStart, xEnd, yEnd);
}

bool epdSupportsPartialRefresh() {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    return false;
#else
    return true;
#endif
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    (void)data;
    (void)oldData;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    epdDisplay(imgBuf);
#else
    int xS = xStart / 8;
    int xE = (xEnd - 1) / 8;
    int width = xE - xS + 1;
    int count = width * (yEnd - yStart);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x80);

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x80);

    epdSendCommand(0x44);  // Set RAM X address range
    epdSendData(xS & 0xFF);
    epdSendData(xE & 0xFF);

    epdSendCommand(0x45);  // Set RAM Y address range
    epdSendData(yStart & 0xFF);
    epdSendData((yStart >> 8) & 0xFF);
    epdSendData((yEnd - 1) & 0xFF);
    epdSendData(((yEnd - 1) >> 8) & 0xFF);

    epdSendCommand(0x4E);  // Set RAM X address counter
    epdSendData(xS & 0xFF);

    epdSendCommand(0x4F);  // Set RAM Y address counter
    epdSendData(yStart & 0xFF);
    epdSendData((yStart >> 8) & 0xFF);

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int i = 0; i < count; i++)
        epdSendData(data[i]);

    if (oldData) {
        epdSendCommand(0x4E);  // Set RAM X address counter
        epdSendData(xS & 0xFF);

        epdSendCommand(0x4F);  // Set RAM Y address counter
        epdSendData(yStart & 0xFF);
        epdSendData((yStart >> 8) & 0xFF);

        epdSendCommand(0x26);  // Write old/secondary RAM for stable partial inversion
        for (int i = 0; i < count; i++)
            epdSendData(oldData[i]);
    }

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xFF);     //   Partial update sequence
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
}

// ── EPD sleep ───────────────────────────────────────────────

void epdSleep() {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdSendCommand(0x07);
    epdSendData(0xA5);
    delay(200);
#else
    epdSendCommand(0x10);  // Deep Sleep Mode
    epdSendData(0x01);     //   Enter deep sleep
    delay(200);
#endif
}

#elif defined(EPD_PANEL_42_WFT)
// Shared WFT042-compatible path for Waveshare WFT0CZ15 and WFH0420CZ35
// families. Validate panel variant, color mapping, and resolution on hardware.
// ── WFT042 4.2" Panel (Waveshare-compatible, supports BW and tricolor via EPD_BPP) ──

#include "epd_wft.h"

// ── GPIO initialization ─────────────────────────────────────

void gpioInit() {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    EPD_initSPI();
}

// ── EPD full init ──

void epdInit() {
    Serial.printf("[EPD-WFT042] controller init start (%dx%d, %d bpp)\n",
                  EPD_WIDTH, EPD_HEIGHT, EPD_BPP);
#if EPD_BPP >= 2
    EPD_dispIndex = 1; // tricolor mode
#else
    EPD_dispIndex = 0; // BW mode
#endif
    EPD_dispInit();
    Serial.println("[EPD-WFT042] controller init sequence returned");
}

// ── EPD fast init ──

void epdInitFast() { epdInit(); }

// ── BW full refresh ──

void epdDisplay(const uint8_t *image) {
    const unsigned long refreshStartedAt = millis();
    Serial.printf("[EPD-WFT042] monochrome full refresh start, %d bytes\n",
                  IMG_BUF_LEN);
    int w = EPD_WIDTH / 8;
    epdInit();

    EPD_SendCommand(0x10);
    delay(2);
    for (int i = 0; i < w * EPD_HEIGHT; i++) {
        EPD_SendData(0xFF);
    }

    EPD_SendCommand(0x13);
    delay(2);
    for (int i = 0; i < w * EPD_HEIGHT; i++) {
        EPD_SendData(image[i]);
    }

    EPD_dispMass[EPD_dispIndex].show();
    Serial.printf("[EPD-WFT042] monochrome refresh complete after %lu ms\n",
                  millis() - refreshStartedAt);
}

// ── Deep clear (multi-cycle flush) ──

void epdDisplayDeepClear(const uint8_t *image) {
    epdDisplay(image);
}

// ── Tricolor display (2bpp packed data) ──

void epdDisplay2bpp(const uint8_t *image2bpp) {
#if EPD_BPP >= 2
    if (!image2bpp) {
        Serial.println("[EPD-WFT042] 2bpp full refresh aborted: null image");
        return;
    }
    const unsigned long refreshStartedAt = millis();
    Serial.printf("[EPD-WFT042] 2bpp full refresh start, %d bytes\n",
                  COLOR_BUF_LEN);
    int w = EPD_WIDTH / 8;
    int total = w * EPD_HEIGHT;
    epdInit();

    uint8_t *blackBuf = (uint8_t *)malloc(total);
    uint8_t *redBuf = (uint8_t *)malloc(total);
    if (!blackBuf || !redBuf) {
        Serial.println("[EPD] 2bpp buffer alloc failed");
        free(blackBuf);
        free(redBuf);
        return;
    }
    memset(blackBuf, 0xFF, total);
    memset(redBuf, 0x00, total);

    for (int y = 0; y < EPD_HEIGHT; y++) {
        for (int x = 0; x < EPD_WIDTH; x++) {
            int index = (y * EPD_WIDTH + x) / 4;
            int bitOffset = ((y * EPD_WIDTH + x) % 4) * 2;
            uint8_t color = (image2bpp[index] >> (6 - bitOffset)) & 0x03;
            if (color == 0x02) color = 0x01; // map yellow to red

            int bufIndex = y * w + (x / 8);
            int bitPos = x % 8;

            if (color == 0x00) { // Black
#if defined(EPD_PANEL_42_BLOZIENDORTYPEB)
                blackBuf[bufIndex] |= (0x80 >> bitPos);
                redBuf[bufIndex] |= (0x80 >> bitPos);
#else
                blackBuf[bufIndex] &= ~(0x80 >> bitPos);
                redBuf[bufIndex] |= (0x80 >> bitPos);
#endif
            } else if (color == 0x01) {
#if defined(EPD_PANEL_42_BLOZIENDORTYPEB)
                blackBuf[bufIndex] &= ~(0x80 >> bitPos);
                redBuf[bufIndex] &= ~(0x80 >> bitPos);
#else
                redBuf[bufIndex] |= (0x80 >> bitPos);
                blackBuf[bufIndex] |= (0x80 >> bitPos);
#endif
            } else {
#if defined(EPD_PANEL_42_BLOZIENDORTYPEB)
                blackBuf[bufIndex] &= ~(0x80 >> bitPos);
                redBuf[bufIndex] |= (0x80 >> bitPos);
#else
                blackBuf[bufIndex] |= (0x80 >> bitPos);
                redBuf[bufIndex] &= ~(0x80 >> bitPos);
#endif
            }
        }
    }

    EPD_SendCommand(0x10);
    delay(2);
    for (int i = 0; i < total; i++) {
        EPD_SendData(blackBuf[i]);
    }

    EPD_SendCommand(0x13);
    delay(2);
    for (int i = 0; i < total; i++) {
        EPD_SendData(redBuf[i]);
    }

    free(blackBuf);
    free(redBuf);

    EPD_dispMass[EPD_dispIndex].show();
    Serial.printf("[EPD-WFT042] 2bpp refresh complete after %lu ms\n",
                  millis() - refreshStartedAt);
#else
    epdDisplay(image2bpp);
#endif
}

// ── Fast refresh (same as full for WFT panel) ──

void epdDisplayFast(const uint8_t *image) {
    epdDisplay(image);
}

bool epdSupportsPartialRefresh() {
    return false;
}

// ── Partial display (not supported on LG, fallback to full) ──

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    (void)xStart; (void)yStart; (void)xEnd; (void)yEnd;
    epdDisplay(data);
}

// ── Sleep ──

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
    (void)oldData;
    epdPartialDisplay(data, xStart, yStart, xEnd, yEnd);
}

void epdSleep() {
    EPD_SendCommand(0x02); // power off
    EPD_WaitUntilIdle();
    EPD_Send_1(0x07, 0xA5); // deep sleep
}

#else
// ── Other panel sizes: GxEPD2 (hardware SPI) ─────────────────

#include <SPI.h>
#include <GxEPD2_BW.h>

#ifndef EPD_GXEPD2_SPI_HZ
#define EPD_GXEPD2_SPI_HZ 4000000
#endif

#if defined(EPD_PANEL_42_BLOZIENDOR_BWR) || defined(EPD_PANEL_42_HINK_E042A31_BWR)
#define INKSIGHT_GDEY042Z98_BWR_PANEL
#include <GxEPD2_3C.h>
#include <gdey3c/GxEPD2_420c_GDEY042Z98.h>
GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT / 4> display(
    GxEPD2_420c_GDEY042Z98(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_213_HINK_E0213A127_BWR)
#define INKSIGHT_HINK_E0213A127_BWR_PANEL
#include <GxEPD2_3C.h>
#include <epd3c/GxEPD2_213_Z98c.h>
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT / 4> display(
    GxEPD2_213_Z98c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_213_A1360069_IL0373_BWR)
#define INKSIGHT_A1360069_IL0373_BWR_PANEL
#include <GxEPD2_3C.h>
#include <epd3c/GxEPD2_213c.h>
GxEPD2_3C<GxEPD2_213c, GxEPD2_213c::HEIGHT / 4> display(
    GxEPD2_213c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#else
  #if defined(EPD_PANEL_42_GXEPD2_T81)
  #include <gdey/GxEPD2_420_GDEY042T81.h>
  GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT / 4> display(
      GxEPD2_420_GDEY042T81(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_42_GXEPD2_GYE042A87)
  #include <other/GxEPD2_420_GYE042A87.h>
  GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(
      GxEPD2_420_GYE042A87(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_42_GXEPD2_420)
  #include <epd/GxEPD2_420.h>
  GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT / 4> display(
      GxEPD2_420(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_42_GXEPD2_M01)
  #include <epd/GxEPD2_420_M01.h>
  GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT / 4> display(
      GxEPD2_420_M01(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_29)
  #include <gdey/GxEPD2_290_GDEY029T94.h>
  GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(
      GxEPD2_290_GDEY029T94(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  static uint8_t rotated_buffer[IMG_BUF_LEN];

  static bool is_black_pixel(const uint8_t* buffer, int width, int x, int y) {
      int row_bytes = (width + 7) / 8;
      return (buffer[y * row_bytes + x / 8] & (0x80 >> (x % 8))) == 0;
  }

  static void set_black_pixel(uint8_t* buffer, int width, int x, int y) {
      int row_bytes = (width + 7) / 8;
      buffer[y * row_bytes + x / 8] &= ~(0x80 >> (x % 8));
  }

  static void rotate_landscape_to_panel(const uint8_t* source) {
      memset(rotated_buffer, 0xFF, sizeof(rotated_buffer));
      for (int src_y = 0; src_y < H; src_y++) {
          for (int src_x = 0; src_x < W; src_x++) {
              if (!is_black_pixel(source, W, src_x, src_y)) continue;
              int dst_x = src_y;
              int dst_y = W - 1 - src_x;
              set_black_pixel(rotated_buffer, GxEPD2_290_GDEY029T94::WIDTH, dst_x, dst_y);
          }
      }
  }
  // Shared GDEQ0583T31 / UC8179 profile (5.83", 648x480); use full refresh
  // because this panel family can retain ghosting with partial-refresh LUTs.
  #elif defined(EPD_PANEL_583_UC8179)
  #include <gdeq/GxEPD2_583_GDEQ0583T31.h>
  GxEPD2_BW<GxEPD2_583_GDEQ0583T31, GxEPD2_583_GDEQ0583T31::HEIGHT / 4> display(
      GxEPD2_583_GDEQ0583T31(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_583)
  #include <gdeq/GxEPD2_583_GDEQ0583T31.h>
  GxEPD2_BW<GxEPD2_583_GDEQ0583T31, GxEPD2_583_GDEQ0583T31::HEIGHT / 4> display(
      GxEPD2_583_GDEQ0583T31(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #elif defined(EPD_PANEL_75)
  #include <epd/GxEPD2_750_T7.h>
  GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT / 4> display(
      GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  #else
  #error "No EPD panel type defined. Use a supported EPD_PANEL_* build flag."
  #endif
#endif

#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL)
static uint8_t gdey042z98BlackPlane[IMG_BUF_LEN];
static uint8_t gdey042z98ColorPlane[IMG_BUF_LEN];

static void packGdey042z98MonoPlanes(const uint8_t *image) {
    memcpy(gdey042z98BlackPlane, image, sizeof(gdey042z98BlackPlane));
    for (int i = 0; i < IMG_BUF_LEN; i++) {
        gdey042z98ColorPlane[i] = (uint8_t)~image[i];
    }
}

static void packGdey042z98ColorPlanes(const uint8_t *image2bpp) {
    memset(gdey042z98BlackPlane, 0xFF, sizeof(gdey042z98BlackPlane));
    memset(gdey042z98ColorPlane, 0x00, sizeof(gdey042z98ColorPlane));
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int pixelIndex = y * W + x;
            uint8_t color = (image2bpp[pixelIndex / 4] >> (6 - (pixelIndex % 4) * 2)) & 0x03;
            uint8_t mask = 0x80 >> (x % 8);
            int byteIndex = y * ROW_BYTES + x / 8;
            if (color == 0x00) {
                gdey042z98BlackPlane[byteIndex] &= (uint8_t)~mask;
                gdey042z98ColorPlane[byteIndex] |= mask;
            } else if (color == 0x01 || color == 0x02) {
                gdey042z98ColorPlane[byteIndex] |= mask;
            }
        }
    }
}
#endif

#if defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
constexpr int A1360069_NATIVE_WIDTH = GxEPD2_213c::WIDTH;
constexpr int A1360069_NATIVE_HEIGHT = GxEPD2_213c::HEIGHT;
constexpr int A1360069_PLANE_BYTES =
    (A1360069_NATIVE_WIDTH / 8) * A1360069_NATIVE_HEIGHT;
static uint8_t a1360069BlackPlane[A1360069_PLANE_BYTES];
static uint8_t a1360069ColorPlane[A1360069_PLANE_BYTES];

static void packA1360069Planes(const uint8_t *monoImage,
                               const uint8_t *colorImage) {
    memset(a1360069BlackPlane, 0xFF, sizeof(a1360069BlackPlane));
    memset(a1360069ColorPlane, 0xFF, sizeof(a1360069ColorPlane));

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int nativeX = y;
            const int nativeY = W - 1 - x;
            const int planeIndex =
                nativeY * (A1360069_NATIVE_WIDTH / 8) + nativeX / 8;
            const uint8_t mask = static_cast<uint8_t>(0x80 >> (nativeX % 8));
            const int pixelIndex = y * W + x;
            const uint8_t color = colorImage
                ? (colorImage[pixelIndex / 4] >> (6 - (pixelIndex % 4) * 2)) & 0x03
                : 1;
            const bool black = colorImage
                ? color == 0
                : (monoImage[y * ROW_BYTES + x / 8] & (0x80 >> (x % 8))) == 0;
            const bool red = colorImage && color == 3;

            if (black) {
                a1360069BlackPlane[planeIndex] &= static_cast<uint8_t>(~mask);
            }
            if (red) {
                a1360069ColorPlane[planeIndex] &= static_cast<uint8_t>(~mask);
            }
        }
    }
}
#endif

#if defined(INKSIGHT_HINK_E0213A127_BWR_PANEL)
constexpr int HINK_E0213A127_NATIVE_WIDTH = GxEPD2_213_Z98c::WIDTH;
constexpr int HINK_E0213A127_NATIVE_HEIGHT = GxEPD2_213_Z98c::HEIGHT;
constexpr int HINK_E0213A127_PLANE_BYTES =
    (HINK_E0213A127_NATIVE_WIDTH / 8) * HINK_E0213A127_NATIVE_HEIGHT;
static uint8_t hinkE0213a127BlackPlane[HINK_E0213A127_PLANE_BYTES];
static uint8_t hinkE0213a127ColorPlane[HINK_E0213A127_PLANE_BYTES];

static void packHinkE0213a127Planes(const uint8_t *monoImage,
                                    const uint8_t *colorImage) {
    memset(hinkE0213a127BlackPlane, 0xFF, sizeof(hinkE0213a127BlackPlane));
    memset(hinkE0213a127ColorPlane, 0xFF, sizeof(hinkE0213a127ColorPlane));

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int nativeX = y;
            const int nativeY = W - 1 - x;
            const int planeIndex =
                nativeY * (HINK_E0213A127_NATIVE_WIDTH / 8) + nativeX / 8;
            const uint8_t mask = static_cast<uint8_t>(0x80 >> (nativeX % 8));
            const bool black = colorImage
                ? ((colorImage[(y * W + x) / 4] >>
                    (6 - ((y * W + x) % 4) * 2)) & 0x03) == 0
                : (monoImage[y * ROW_BYTES + x / 8] & (0x80 >> (x % 8))) == 0;
            const bool red = colorImage &&
                ((colorImage[(y * W + x) / 4] >>
                  (6 - ((y * W + x) % 4) * 2)) & 0x03) == 3;

            if (black) {
                hinkE0213a127BlackPlane[planeIndex] &=
                    static_cast<uint8_t>(~mask);
            }
            if (!red) {
                hinkE0213a127ColorPlane[planeIndex] |= mask;
            } else {
                hinkE0213a127ColorPlane[planeIndex] &=
                    static_cast<uint8_t>(~mask);
            }
        }
    }
}
#endif

static bool _initialized = false;
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
static bool _needs_full_refresh_write = true;
#endif
static const uint8_t DISPLAY_ROTATION =
#if defined(EPD_PANEL_42_GXEPD2_T81) || defined(EPD_PANEL_42_GXEPD2_GYE042A87) || defined(EPD_PANEL_42_GXEPD2_420) || defined(EPD_PANEL_42_GXEPD2_M01)
    0;
#else
    1;
#endif

void gpioInit() {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
}

void epdInit() {
    const unsigned long initStartedAt = millis();
    Serial.printf("[EPD-GxEPD2] init start (%dx%d)\n", W, H);
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL) || defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    if (!_initialized) {
        display.epd2.selectSPI(SPI, SPISettings(EPD_GXEPD2_SPI_HZ, MSBFIRST, SPI_MODE0));
        display.init(0);
        _initialized = true;
    }
#else
    if (!_initialized) {
        display.epd2.selectSPI(SPI, SPISettings(EPD_GXEPD2_SPI_HZ, MSBFIRST, SPI_MODE0));
        display.init(0);
        display.setRotation(DISPLAY_ROTATION);
        _initialized = true;
    }
#endif
    Serial.printf("[EPD-GxEPD2] init complete after %lu ms\n",
                  millis() - initStartedAt);
}

void epdInitFast() {
    epdInit();
}

void epdDisplay(const uint8_t *image) {
    if (!image) {
        Serial.println("[EPD-GxEPD2] full-screen refresh aborted: null image");
        return;
    }
    const unsigned long refreshStartedAt = millis();
    Serial.printf("[EPD-GxEPD2] full-screen write start, %d bytes\n", IMG_BUF_LEN);
    epdInit();
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL)
    packGdey042z98MonoPlanes(image);
    display.writeScreenBuffer(0xFF);
    display.writeImage(gdey042z98BlackPlane, gdey042z98ColorPlane, 0, 0, W, H, false, false, false);
    Serial.println("[EPD-GxEPD2] image data buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#elif defined(INKSIGHT_HINK_E0213A127_BWR_PANEL)
    packHinkE0213a127Planes(image, nullptr);
    display.writeScreenBuffer(0xFF);
    display.writeImage(hinkE0213a127BlackPlane, hinkE0213a127ColorPlane,
                       0, 0, HINK_E0213A127_NATIVE_WIDTH,
                       HINK_E0213A127_NATIVE_HEIGHT, false, false, false);
    Serial.println("[EPD-GxEPD2] native-orientation image buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#elif defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    packA1360069Planes(image, nullptr);
    display.epd2.writeScreenBuffer(0xFF, 0xFF);
    display.writeImage(a1360069BlackPlane, a1360069ColorPlane,
                       0, 0, A1360069_NATIVE_WIDTH, A1360069_NATIVE_HEIGHT,
                       false, false, false);
    Serial.println("[EPD-GxEPD2] A1360069 native planes buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#else
#if defined(EPD_PANEL_29)
    rotate_landscape_to_panel(image);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
#elif defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    if (_needs_full_refresh_write) {
        display.epd2.writeImageForFullRefresh(image, 0, 0, W, H, false, false, false);
        _needs_full_refresh_write = false;
    } else {
        display.writeImage(image, 0, 0, W, H, false, false, false);
    }
#else
    display.writeImage(image, 0, 0, W, H, false, false, false);
#endif
    Serial.println("[EPD-GxEPD2] image data buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#endif
    Serial.printf("[EPD-GxEPD2] full-screen refresh sequence returned after %lu ms\n",
                  millis() - refreshStartedAt);
}

void epdDisplayFast(const uint8_t *image) {
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL) || defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    epdDisplay(image);
    return;
#endif
#if defined(EPD_PANEL_583_UC8179)
    // 583 UC8179: always full refresh (GxEPD2 refresh(false)); avoids partial LUT ghosting.
    epdDisplay(image);
    return;
#endif
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    epdDisplay(image);
    return;
#endif
    epdInit();
#if defined(EPD_PANEL_29)
    rotate_landscape_to_panel(image);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
#else
    display.writeImage(image, 0, 0, W, H, false, false, true);
#endif
    display.refresh(true);
    display.powerOff();
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL)
    epdInit();
    packGdey042z98ColorPlanes(image2bpp);
    display.writeScreenBuffer(0xFF);
    display.writeImage(gdey042z98BlackPlane, gdey042z98ColorPlane, 0, 0, W, H, false, false, false);
    display.refresh(false);
    display.powerOff();
#elif defined(INKSIGHT_HINK_E0213A127_BWR_PANEL)
    if (!image2bpp) {
        Serial.println("[EPD-GxEPD2] 2bpp refresh aborted: null image");
        return;
    }
    epdInit();
    packHinkE0213a127Planes(nullptr, image2bpp);
    display.writeScreenBuffer(0xFF);
    display.writeImage(hinkE0213a127BlackPlane, hinkE0213a127ColorPlane,
                       0, 0, HINK_E0213A127_NATIVE_WIDTH,
                       HINK_E0213A127_NATIVE_HEIGHT, false, false, false);
    Serial.println("[EPD-GxEPD2] 2bpp planes buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#elif defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    if (!image2bpp) {
        Serial.println("[EPD-GxEPD2] 2bpp refresh aborted: null image");
        return;
    }
    epdInit();
    packA1360069Planes(nullptr, image2bpp);
    display.epd2.writeScreenBuffer(0xFF, 0xFF);
    display.writeImage(a1360069BlackPlane, a1360069ColorPlane,
                       0, 0, A1360069_NATIVE_WIDTH, A1360069_NATIVE_HEIGHT,
                       false, false, false);
    Serial.println("[EPD-GxEPD2] A1360069 2bpp planes buffered; refresh start");
    display.refresh(false);
    Serial.println("[EPD-GxEPD2] refresh complete");
    display.powerOff();
#else
    (void)image2bpp;
    epdDisplay(imgBuf);
#endif
}

void epdDisplayDeepClear(const uint8_t *image) {
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL) || defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    epdDisplay(image);
    return;
#endif
    epdInit();

    uint8_t *clearBuf = (uint8_t *)malloc(IMG_BUF_LEN);
    if (clearBuf) {
        for (int pass = 0; pass < 4; pass++) {
            memset(clearBuf, (pass % 2 == 0) ? 0x00 : 0xFF, IMG_BUF_LEN);
            display.writeImage(clearBuf, 0, 0, W, H, false, false, true);
            display.refresh(false);
        }
        free(clearBuf);
        display.powerOff();
    }

    epdDisplay(image);
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    epdPartialDisplayWithOld(data, nullptr, xStart, yStart, xEnd, yEnd);
}

bool epdSupportsPartialRefresh() {
#if defined(EPD_PANEL_29) || defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL) || defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    return false;
#else
    return true;
#endif
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL) || defined(INKSIGHT_A1360069_IL0373_BWR_PANEL)
    (void)oldData;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    epdDisplay(data);
    return;
#endif
    epdInit();
#if defined(EPD_PANEL_29)
    (void)data;
    (void)oldData;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    rotate_landscape_to_panel(imgBuf);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
    display.refresh(true);
#else
    int w = xEnd - xStart;
    int h = yEnd - yStart;
    if (oldData) {
        display.epd2.writeImageAgain(oldData, xStart, yStart, w, h, false, false, true);
        display.writeImage(data, xStart, yStart, w, h, false, false, true);
    } else {
        display.writeImage(data, xStart, yStart, w, h, false, false, true);
        display.epd2.writeImageAgain(data, xStart, yStart, w, h, false, false, true);
    }
    display.refresh(xStart, yStart, w, h);
#endif
    display.powerOff();
}

void epdSleep() {
#if defined(INKSIGHT_GDEY042Z98_BWR_PANEL) || defined(INKSIGHT_HINK_E0213A127_BWR_PANEL)
    display.hibernate();
    _initialized = false;
#else
    display.hibernate();
    _initialized = false;
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    _needs_full_refresh_write = true;
#endif
#endif
}
#endif // EPD_PANEL_42_SSD1683_BW

#endif // display driver selection
