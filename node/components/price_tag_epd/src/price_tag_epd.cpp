#include "price_tag_epd.h"
#include "esp_log.h"

// ====== EAN-13 LUT ======
static const char* EAN_A[10] = {
  "0001101","0011001","0010011","0111101","0100011",
  "0110001","0101111","0111011","0110111","0001011"
};
static const char* EAN_B[10] = {
  "0100111","0110011","0011011","0100001","0011101",
  "0111001","0000101","0010001","0001001","0010111"
};
static const char* EAN_C[10] = {
  "1110010","1100110","1101100","1000010","1011100",
  "1001110","1010000","1000100","1001000","1110100"
};
static const char* PARITY[10] = {
  "AAAAAA", "AABABB", "AABBAB", "AABBBA", "ABAABB",
  "ABBAAB", "ABBBAA", "ABABAB", "ABABBA", "ABBABA"
};

static const char* EPD_TAG = "EPD";

PriceTagEPD::PriceTagEPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy)
: pinCS(cs), pinDC(dc), pinRST(rst), pinBUSY(busy) {}

void PriceTagEPD::begin(uint8_t rotation, bool initial_full_refresh) {
  if (!display) {
    // ĐÚNG kiểu panel 3-màu 2.9"
    display = new GxEPD2_3C<Panel, Panel::HEIGHT>(Panel(pinCS, pinDC, pinRST, pinBUSY));
  }

  // --- QUAN TRỌNG: cấu hình GPIO trước khi driver đụng tới ---
  pinMode(pinCS,   OUTPUT);
  pinMode(pinDC,   OUTPUT);
  pinMode(pinRST,  OUTPUT);
  // BUSY thường là ngõ vào từ panel; nếu thả nổi có thể dùng INPUT_PULLUP
  pinMode(pinBUSY, INPUT);
  digitalWrite(pinCS, HIGH);   // nhả CS mặc định

  // SPI.begin(...) đã được gọi ở main trước khi vào begin()

  ESP_LOGI(EPD_TAG, "display->init() start");
  // init(bitRate=0, initial=initial_full_refresh, reset_duration_ms=10, pulldown_rst=false)
  display->init(0, initial_full_refresh, 10, false);
  display->setRotation(rotation);
  ESP_LOGI(EPD_TAG, "display->init() ok");

  currentRotation = rotation;

  // (tuỳ chọn) 1 vòng refresh rỗng để làm sạch
  display->setFullWindow();
  display->firstPage();
  do {} while (display->nextPage());
  ESP_LOGI(EPD_TAG, "first empty refresh done");
}

Adafruit_GFX* PriceTagEPD::gfx() { return display; }
int16_t PriceTagEPD::width()  const { return display->width(); }
int16_t PriceTagEPD::height() const { return display->height(); }

void PriceTagEPD::drawCenteredText(const String& s, int16_t cx, int16_t yBaseline,
                                   const GFXfont* f, uint16_t color) {
  if (f) display->setFont(f);
  else   display->setFont(); // default

  int16_t x1, y1; uint16_t w, h;
  display->getTextBounds(s, 0, yBaseline, &x1, &y1, &w, &h);
  int16_t x = cx - (int16_t)w / 2;

  display->setTextColor(color);
  display->setCursor(x, yBaseline);
  display->print(s);
  display->setTextColor(GxEPD_BLACK);
}

void PriceTagEPD::drawStrikeThroughText(const String& s, int16_t x, int16_t yBaseline,
                                        const GFXfont* f) {
  if (f) display->setFont(f);
  else   display->setFont();

  display->setTextColor(GxEPD_BLACK);
  display->setCursor(x, yBaseline);
  display->print(s);

  int16_t x1, y1; uint16_t w, h;
  display->getTextBounds(s, x, yBaseline, &x1, &y1, &w, &h);
  int16_t midY = yBaseline - (int16_t)h / 2;
  display->drawLine(x1, midY, x1 + w, midY, GxEPD_BLACK);
}

bool PriceTagEPD::isDigits13(const String& s) const {
  if (s.length() != 13) return false;
  for (int i = 0; i < 13; ++i) if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

void PriceTagEPD::buildEAN13Pattern(const char* digits, String& pattern) {
  pattern.reserve(95);

  // start
  pattern += "101";

  int lead = digits[0] - '0';
  const char* parity = PARITY[lead];

  // left 1..6
  for (int i = 1; i <= 6; ++i) {
    int d = digits[i] - '0';
    pattern += (parity[i-1] == 'A') ? EAN_A[d] : EAN_B[d];
  }

  // center
  pattern += "01010";

  // right 7..12 (C)
  for (int i = 7; i <= 12; ++i) {
    int d = digits[i] - '0';
    pattern += EAN_C[d];
  }

  // end
  pattern += "101";
}

void PriceTagEPD::drawEAN13Bars(int16_t x, int16_t y, const String& pattern) {
  int16_t cursor = x;
  for (int i = 0; i < (int)pattern.length(); ++i) {
    bool isBlack = (pattern[i] == '1');
    int16_t barH = EAN_BAR_HEIGHT;
    if ((i <= 2) || (i >= 45 && i <= 49) || (i >= 92 && i <= 94)) barH += EAN_GUARD_EXTRA;
    if (isBlack) {
      display->fillRect(cursor, y - barH, EAN_MODULE_PX, barH, GxEPD_BLACK);
    }
    cursor += EAN_MODULE_PX;
  }
}

void PriceTagEPD::drawEAN13HumanReadable(int16_t x, int16_t y, const String& digits) {
  display->setFont(); // default
  int16_t totalW = 95 * EAN_MODULE_PX;
  (void)totalW;

  String d0 = digits.substring(0,1);
  String left = digits.substring(1,7);
  String right = digits.substring(7,13);

  int16_t x_d0 = x - 10;
  int16_t x_left = x + 3*EAN_MODULE_PX;
  int16_t x_right = x + (3 + 42 + 5)*EAN_MODULE_PX;
  int16_t baseline = y + EAN_TEXT_GAP + 8;

  display->setCursor(x_d0, baseline); display->print(d0);
  display->setCursor(x_left, baseline); display->print(left);
  display->setCursor(x_right, baseline); display->print(right);
}

void PriceTagEPD::drawSaleBadge(int16_t x, int16_t y, int16_t w, int16_t h, const String& saleTiny)
{
  // --- tuning: padding & khoảng cách 2 dòng ---
  const int16_t PADDING = 6;     // lề trong
  const int16_t V_GAP   = 2;     // khoảng cách SALE <-> số
  const int16_t RADIUS  = 6;     // bo góc
  const int16_t MIN_W   = 70;    // tối thiểu để nhìn cân
  const int16_t MIN_H   = 28;

  // đo dòng 1: "SALE"
  display->setFont(titleFont ? titleFont : &FreeSansBold12pt7b);
  int16_t x1,y1; uint16_t tw1, th1;
  display->getTextBounds("SALE", 0, 0, &x1, &y1, &tw1, &th1);

  // đo dòng 2: số/giá
  display->setFont(smallFont ? smallFont : &FreeSansBold9pt7b);
  int16_t x2,y2; uint16_t tw2, th2;
  const String line2 = saleTiny.length() ? saleTiny : ""; // cho phép rỗng
  display->getTextBounds(line2, 0, 0, &x2, &y2, &tw2, &th2);

  // nếu caller đưa w/h <= 0 → autosize
  if (w <= 0 || h <= 0) {
    uint16_t innerW = max(tw1, tw2);
    uint16_t innerH = th1 + V_GAP + th2;
    w = max<int16_t>(MIN_W, innerW + 2*PADDING);
    h = max<int16_t>(MIN_H, innerH + 2*PADDING);
  }

  // vẽ nền/viền
  display->drawRoundRect(x, y, w, h, RADIUS, GxEPD_BLACK);
  display->fillRoundRect(x+2, y+2, w-4, h-4, RADIUS, GxEPD_RED);

  // vẽ dòng 1: "SALE" (trắng, ở nửa trên)
  display->setFont(titleFont ? titleFont : &FreeSansBold12pt7b);
  display->setTextColor(GxEPD_WHITE);
  // canh giữa theo X, baseline đặt ở nửa trên
  display->getTextBounds("SALE", 0, 0, &x1, &y1, &tw1, &th1);
  int16_t baseTop = y + PADDING + th1;     // đẩy vào trong theo padding
  int16_t cx1     = x + (w - (int16_t)tw1)/2;
  display->setCursor(cx1, baseTop);
  display->print("SALE");

  // vẽ dòng 2: số/giá (trắng, gần đáy)
  display->setFont(smallFont ? smallFont : &FreeSansBold9pt7b);
  display->getTextBounds(line2, 0, 0, &x2, &y2, &tw2, &th2);
  int16_t baseBot = y + h - PADDING;       // sát đáy trong
  int16_t cx2     = x + (w - (int16_t)tw2)/2;
  display->setCursor(cx2, baseBot);
  display->print(line2);

  // trả lại màu đen cho phần còn lại
  display->setTextColor(GxEPD_BLACK);
}



void PriceTagEPD::drawTagLayout(const String& title,
                                const String& saleTiny,
                                const String& codeTop,
                                const String& codeBot,
                                const String& ean13)
{
  const int16_t W = display->width();
  const int16_t H = display->height();
  const int16_t PAD = 10;

  display->fillScreen(GxEPD_WHITE);

  // === Header autosize theo TITLE (pill) ===
  const int16_t R        = 6;   // bo góc
  const int16_t PAD_X    = 8;   // padding ngang bên trong khung
  const int16_t PAD_Y    = 6;   // padding dọc bên trong khung
  const int16_t MIN_H    = 24;  // tối thiểu cho đẹp

  // đo title
  if (titleFont) display->setFont(titleFont); else display->setFont();
  int16_t x1, y1; uint16_t tw, th;
  display->getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);

  // tính kích thước khung
  int16_t barW = tw + 2*PAD_X;
  int16_t barH = max<int16_t>(MIN_H, th + 2*PAD_Y);

  // vẽ khung bo góc, ôm sát chữ
  int16_t barX = PAD;
  int16_t barY = PAD;
  display->drawRoundRect(barX, barY, barW, barH, R, GxEPD_BLACK);

  // in title canh trái trong khung (baseline)
  int16_t baseY = barY + PAD_Y + th;       // baseline chữ nằm trong khung
  int16_t textX = barX + PAD_X;
  display->setTextColor(GxEPD_BLACK);
  display->setCursor(textX, baseY);
  display->print(title);

  if (saleTiny.length() > 0 && saleTiny != "-") {
    // đo bề rộng title
    int16_t x1,y1; uint16_t tw,th;
    display->setFont(titleFont ? titleFont : &FreeSansBold12pt7b);
    display->getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);

    const int16_t GAP_X = 12;     // khoảng cách giữa title và SALE
    int16_t sx = PAD + 6 + (int16_t)tw + GAP_X; // đặt ngay sau title
    int16_t sy = PAD + 3;                          // sát mép trên
    drawSaleBadge(sx, sy, -1, -1, saleTiny);       // autosize
  }



// === Hai dòng giá neo SÁT BÊN DƯỚI, canh phải, sát nhau ===
{
  // tuning nhanh
  const GFXfont* pf = &FreeSansBold12pt7b; // nhỏ gọn
  const int16_t RIGHT_PAD   = 12;         // cách mép phải
  const int16_t SHIFT_X     = 0;          // + sang phải / - sang trái
  const int16_t BOT_MARGIN  = 8;          // cách mép dưới (px)
  const int16_t GAP         = 20;          // khoảng cách 2 dòng (px), nhỏ = sát

  // đo kích thước text để canh phải
  int16_t x1, y1; uint16_t wTop=0, hTop=0, wBot=0, hBot=0;
  if (pf) display->setFont(pf); else display->setFont();
  if (codeTop.length()) display->getTextBounds(codeTop, 0, 0, &x1, &y1, &wTop, &hTop);
  display->getTextBounds(codeBot, 0, 0, &x1, &y1, &wBot, &hBot);

  // dòng dưới (ĐỎ) — neo vào đáy
  {
    int16_t xBot   = W - PAD - RIGHT_PAD - (int16_t)wBot + SHIFT_X;
    int16_t baseBot= H - PAD - BOT_MARGIN;           // baseline sát đáy
    display->setTextColor(GxEPD_RED);
    display->setCursor(xBot, baseBot);
    display->print(codeBot);
    display->setTextColor(GxEPD_BLACK);
  }

  // dòng trên (đen gạch) — đặt ngay TRÊN dòng đỏ, cách GAP px
  if (codeTop.length()) {
    int16_t xTop    = W - PAD - RIGHT_PAD - (int16_t)wTop + SHIFT_X;
    int16_t baseTop = (H - PAD - BOT_MARGIN) - GAP;   // cách dòng dưới đúng GAP
    drawStrikeThroughText(codeTop, xTop, baseTop, pf);
  }
}


  // EAN-13 (nếu hợp lệ) — bản hẹp
  if (isDigits13(ean13)) {
    String pattern; buildEAN13Pattern(ean13.c_str(), pattern);

    const int16_t MODULE = 1;             // module hẹp nhất
    const int     QUIET_MODULES = 7;      // giảm lề để tổng chiều ngang ngắn lại
    const int16_t quiet = QUIET_MODULES * MODULE;

    int16_t bx = PAD + quiet;             // bám trái + lề
    int16_t by = H - PAD - 10;

    // vẽ vạch (rút gọn ngang: MODULE=1)
    int16_t cursor = bx;
    for (int i = 0; i < (int)pattern.length(); ++i) {
      bool on = (pattern[i] == '1');
      int16_t h = EAN_BAR_HEIGHT;
      if ((i <= 2) || (i >= 45 && i <= 49) || (i >= 92 && i <= 94)) h += EAN_GUARD_EXTRA;
      if (on) display->fillRect(cursor, by - h, MODULE, h, GxEPD_BLACK);
      cursor += MODULE;
    }
  }


}

void PriceTagEPD::renderTag(const String& title,
                            const String& saleTiny,
                            const String& codeTop,
                            const String& codeBot,
                            const String& ean13)
{
  display->setFullWindow();
  display->firstPage();
  do {
    drawTagLayout(title, saleTiny, codeTop, codeBot, ean13);
  } while (display->nextPage());
}
