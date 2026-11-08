#include "display.h"

#include "config.h"

#include <string.h>

namespace {

constexpr uint8_t kBlack = 0;
constexpr uint8_t kWhite = 1;
constexpr uint8_t kYellow = 2;
constexpr uint8_t kRed = 3;

struct Glyph {
    char character;
    const char *rows;
};

const Glyph kFont[] = {
    {'A', "01110100011000111111100011000110001"},
    {'B', "11110100011000111110100011000111110"},
    {'C', "01111100001000010000100001000001111"},
    {'D', "11110100011000110001100011000111110"},
    {'E', "11111100001000011110100001000011111"},
    {'F', "11111100001000011110100001000010000"},
    {'G', "01111100001000010111100011000101111"},
    {'H', "10001100011000111111100011000110001"},
    {'I', "11111001000010000100001000010011111"},
    {'J', "00111000100001000010000101001001100"},
    {'K', "10001100101010011000101001001010001"},
    {'L', "10000100001000010000100001000011111"},
    {'M', "10001110111010110101100011000110001"},
    {'N', "10001110011010110011100011000110001"},
    {'O', "01110100011000110001100011000101110"},
    {'P', "11110100011000111110100001000010000"},
    {'Q', "01110100011000110001101011001001101"},
    {'R', "11110100011000111110101001001010001"},
    {'S', "01111100001000001110000010000111110"},
    {'T', "11111001000010000100001000010000100"},
    {'U', "10001100011000110001100011000101110"},
    {'V', "10001100011000110001100010101000100"},
    {'W', "10001100011000110101101011101110001"},
    {'X', "10001100010101000100010101000110001"},
    {'Y', "10001100010101000100001000010000100"},
    {'Z', "11111000010001000100010001000011111"},
    {'0', "01110100011001110101110011000101110"},
    {'1', "00100011000010000100001000010001110"},
    {'2', "01110100010000100010001000100011111"},
    {'3', "11110000010000101110000010000111110"},
    {'4', "00010001100101010010111110001000010"},
    {'5', "11111100001000011110000010000111110"},
    {'6', "01110100001000011110100011000101110"},
    {'7', "11111000010001000100010000100001000"},
    {'8', "01110100011000101110100011000101110"},
    {'9', "01110100011000101111000010000101110"},
    {':', "00000001000001000000001000001000000"},
    {'-', "00000000000000011111000000000000000"},
    {'>', "10000010000001000001000001010001000"},
    {'<', "00001000100010000100001000010000010"},
    {'^', "00100010101000100000000000000000000"},
    {'v', "00000000000000000000010001010100010"},
    {'/', "00001000100001000100010001000010000"},
    {'.', "00000000000000000000000000000110000"},
    {'?', "01110100010000100010001000000000100"},
};

const char *glyphRows(char character) {
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    for (const Glyph &glyph : kFont) {
        if (glyph.character == character) {
            return glyph.rows;
        }
    }
    return "00000000000000000000000000000000000";
}

void clearFrame(uint8_t color) {
    const bool black = color == kBlack;
    memset(imgBuf, black ? 0x00 : 0xFF, IMG_BUF_LEN);
#if EPD_BPP >= 2
    if (ensureColorBuf()) {
        memset(colorBuf, static_cast<uint8_t>(color * 0x55), COLOR_BUF_LEN);
    }
#endif
}

void setPixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= W || y < 0 || y >= H) {
        return;
    }

    const int monoIndex = y * ROW_BYTES + x / 8;
    const uint8_t monoMask = static_cast<uint8_t>(0x80 >> (x % 8));
    const bool black = color == kBlack;
    if (black) {
        imgBuf[monoIndex] &= static_cast<uint8_t>(~monoMask);
    } else {
        imgBuf[monoIndex] |= monoMask;
    }

#if EPD_BPP >= 2
    if (colorBuf) {
        const int pixelIndex = y * W + x;
        const int byteIndex = pixelIndex / 4;
        const int shift = 6 - (pixelIndex % 4) * 2;
        const uint8_t mask = static_cast<uint8_t>(0x03 << shift);
        colorBuf[byteIndex] = static_cast<uint8_t>(
            (colorBuf[byteIndex] & static_cast<uint8_t>(~mask)) |
            ((color & 0x03) << shift));
    }
#endif
}

void fillRect(int x, int y, int width, int height, uint8_t color) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            setPixel(px, py, color);
        }
    }
}

void drawRect(int x, int y, int width, int height, uint8_t color) {
    fillRect(x, y, width, 1, color);
    fillRect(x, y + height - 1, width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(x + width - 1, y, 1, height, color);
}

void drawText(const char *text, int x, int y, int scale, uint8_t color) {
    int cursor = x;
    for (const char *character = text; *character; ++character) {
        const char *rows = glyphRows(*character);
        if (*character != ' ') {
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if (rows[row * 5 + column] == '1') {
                        fillRect(cursor + column * scale, y + row * scale,
                                 scale, scale, color);
                    }
                }
            }
        }
        cursor += 6 * scale;
    }
}

void drawCenteredText(const char *text, int y, int scale, uint8_t color) {
    const int textWidth = static_cast<int>(strlen(text)) * 6 * scale - scale;
    drawText(text, (W - textWidth) / 2, y, scale, color);
}

void drawHeader(const char *title, uint8_t background) {
    const int headerHeight = H / 6;
    const int titleScale = H >= 250 ? 3 : (H >= 150 ? 2 : 1);
    fillRect(0, 0, W, headerHeight, background);
    drawCenteredText(title, headerHeight > 32 ? 6 : 1, titleScale,
                     background == kBlack ? kWhite : kBlack);
    char dimensions[24];
    snprintf(dimensions, sizeof(dimensions), "%dX%d", W, H);
    drawCenteredText(dimensions, headerHeight - 8, 1,
                     background == kBlack ? kWhite : kBlack);
}

void renderPaletteSlide() {
    clearFrame(kWhite);
    drawHeader("PALETTE TEST", kWhite);
    const int top = H / 6 + 4;
    const int bandHeight = (H - top) * 3 / 20;
    const uint8_t colors[] = {kBlack, kWhite, kYellow, kRed};
    const char *labels[] = {"BLACK", "WHITE", "YELLOW", "RED"};
    const int labelScale = bandHeight >= 16 ? 2 : 1;
    for (int band = 0; band < 4; ++band) {
        const int y = top + band * bandHeight;
        fillRect(0, y, W, bandHeight, colors[band]);
        const uint8_t textColor = (colors[band] == kBlack) ? kWhite : kBlack;
        drawText(labels[band], 8, y + (bandHeight - 7 * labelScale) / 2,
                 labelScale, textColor);
    }
    const int patternY = top + 4 * bandHeight;
    const int patternHeight = H - patternY;
    for (int y = patternY; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const uint8_t color = ((x / 8 + y / 8) % 2 == 0) ? kBlack : kWhite;
            setPixel(x, y, color);
        }
    }
    if (patternHeight > 30) {
        drawText("CHECK", 8, patternY + 4, 2, kRed);
    }
    drawRect(0, 0, W, H, kBlack);
}

void renderSolidSlide(uint8_t color, const char *label) {
    clearFrame(color);
    drawRect(2, 2, W - 4, H - 4, color == kBlack ? kWhite : kBlack);
    const int panelWidth = W * 3 / 4;
    const int panelHeight = H / 3;
    const int panelX = (W - panelWidth) / 2;
    const int panelY = (H - panelHeight) / 2;
    fillRect(panelX, panelY, panelWidth, panelHeight,
             color == kBlack ? kBlack : kWhite);
    drawRect(panelX, panelY, panelWidth, panelHeight,
             color == kBlack ? kWhite : kBlack);
    drawCenteredText(label, panelY + panelHeight / 3, 2,
                     color == kBlack ? kWhite : kBlack);
    char dimensions[24];
    snprintf(dimensions, sizeof(dimensions), "%dX%d", W, H);
    drawCenteredText(dimensions, panelY + panelHeight + H / 24, 1,
                     color == kBlack ? kWhite : kBlack);
}

void renderOrientationSlide() {
    clearFrame(kWhite);
    drawHeader("ORIENTATION", kWhite);
    const int margin = W / 16;
    const int top = H / 5;
    const int bottom = H - H / 8;
    drawRect(margin, top, W - 2 * margin, bottom - top, kBlack);
    fillRect(margin + 1, top + 1, W - 2 * margin - 2, 5, kRed);
    fillRect(margin + 1, bottom - 6, W - 2 * margin - 2, 5, kBlack);
    const int topLabelY = top + 8;
    const int bottomLabelY = bottom - 14;
    drawCenteredText("TOP ^", topLabelY, 1, kBlack);
    drawCenteredText("BOTTOM v", bottomLabelY, 1, kBlack);

    const int centerY = (top + bottom) / 2;
    const int centerX = W / 2;
    const int axisTop = topLabelY + 11;
    const int axisBottom = bottomLabelY - 4;
    fillRect(centerX - 2, axisTop, 5, axisBottom - axisTop, kBlack);
    const int sideLabelY = centerY - 10;
    const int sideLabelWidth = 5 * 6 - 1;
    drawText("LEFT", margin + 4, sideLabelY, 1, kBlack);
    drawText("RIGHT", W - margin - sideLabelWidth - 4, sideLabelY, 1, kBlack);
    fillRect(margin + 12, centerY - 2, W - 2 * margin - 24, 5, kBlack);
    drawRect(0, 0, W, H, kBlack);
}

} // namespace

void renderTestSlide(uint8_t slide) {
    switch (slide % TEST_SLIDE_COUNT) {
    case 0:
        renderPaletteSlide();
        break;
    case 1:
        renderSolidSlide(kBlack, "BLACK");
        break;
    case 2:
        renderSolidSlide(kWhite, "WHITE");
        break;
    case 3:
        renderSolidSlide(kYellow, "YELLOW");
        break;
    case 4:
        renderSolidSlide(kRed, "RED");
        break;
    default:
        renderOrientationSlide();
        break;
    }
}
