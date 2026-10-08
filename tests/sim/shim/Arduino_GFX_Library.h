#pragma once
#include "Arduino.h"
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define RGB565_BLACK RGB565(0, 0, 0)
#define RGB565_WHITE RGB565(255, 255, 255)
#define RGB565_RED RGB565(255, 0, 0)
#define RGB565_YELLOW RGB565(255, 255, 0)
extern long g_draw_calls, g_pixels, g_oob; extern int g_draw_cost; extern std::vector<std::string> g_text;
class Arduino_DataBus {};
class Arduino_HWSPI : public Arduino_DataBus { public: Arduino_HWSPI(int, int, int, int, int) {} };
class Arduino_GFX
{
    int w = 240, h = 320;
public:
    std::vector<uint16_t> fb = std::vector<uint16_t>(320 * 320, 0);
    bool begin(long) { return true; }
    void setRotation(uint8_t r) { if (r & 1) { w = 320; h = 240; } else { w = 240; h = 320; } }
    void invertDisplay(bool) {}
    int16_t width() { return w; }
    int16_t height() { return h; }
    void fillScreen(uint16_t c) { std::fill(fb.begin(), fb.end(), c); }
    void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
    void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
    void setTextSize(uint8_t) {} void setTextColor(uint16_t) {} void setCursor(int16_t, int16_t) {} void setTextWrap(bool) {}
    void print(const String &t) { g_text.push_back(t.s); }
    void draw16bitBeRGBBitmap(int16_t x, int16_t y, uint16_t *bm, int16_t bw, int16_t bh)
    {
        g_draw_calls++; g_ms += g_draw_cost; // decoding + drawing takes real time on the board
        for (int j = 0; j < bh; j++) for (int i = 0; i < bw; i++) { int px = x + i, py = y + j; if (px < 0 || py < 0 || px >= w || py >= h) { g_oob++; continue; } fb[py * 320 + px] = bm[j * bw + i]; g_pixels++; }
    }
};
class Arduino_ILI9341 : public Arduino_GFX { public: Arduino_ILI9341(Arduino_DataBus *) {} };
