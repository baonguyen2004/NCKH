#pragma once
#include <Arduino.h>

// BẮT BUỘC 3-MÀU
#define EPD_PANEL_3C

#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#ifdef EPD_PANEL_3C
  #include <GxEPD2_3C.h>
  // === PANEL ĐÚNG VỚI SKETCH ARDUINO CỦA ÔNG ===
  using Panel = GxEPD2_213_Z98c;                    // SSD1680, 2.13" 3C, 250x122
  template<typename T> using EpdDrv = GxEPD2_3C<T, T::HEIGHT>;
#else
  #error "Bảng này phải là 3 màu. Đừng tắt EPD_PANEL_3C."
#endif

class PriceTagEPD {
public:
  PriceTagEPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy);
  void begin(uint8_t rotation = 3, bool initial_full_refresh = true);

  void renderTag(const String& title,
                 const String& saleTiny,
                 const String& codeTop,
                 const String& codeBot,
                 const String& ean13);

  Adafruit_GFX* gfx();
  int16_t width()  const;
  int16_t height() const;

private:
  void drawCenteredText(const String&, int16_t cx, int16_t yBase,
                        const GFXfont* f, uint16_t color);
  void drawStrikeThroughText(const String&, int16_t x, int16_t yBase,
                             const GFXfont* f);
  bool isDigits13(const String& s) const;
  void buildEAN13Pattern(const char* digits, String& pattern);
  void drawEAN13Bars(int16_t x, int16_t y, const String& pattern);
  void drawEAN13HumanReadable(int16_t x, int16_t y, const String& digits);
  void drawSaleBadge(int16_t x, int16_t y, int16_t w, int16_t h, const String& saleTiny);
  void drawTagLayout(const String&, const String&, const String&, const String&, const String&);

private:
  int8_t pinCS, pinDC, pinRST, pinBUSY;
  uint8_t currentRotation = 3;
  EpdDrv<Panel>* display = nullptr;

  const GFXfont* smallFont = &FreeSansBold9pt7b;
  const GFXfont* titleFont = &FreeSansBold12pt7b;
  const GFXfont* priceFont = &FreeSansBold12pt7b;

  static constexpr int EAN_MODULE_PX   = 2;
  static constexpr int EAN_BAR_HEIGHT  = 40;
  static constexpr int EAN_GUARD_EXTRA = 6;
  static constexpr int EAN_TEXT_GAP    = 4;
};
