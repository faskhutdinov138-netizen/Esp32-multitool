// ============================================================
//  Linux — многофункциональное устройство на ESP32
//  Дисплей: TFT_eSPI 240x320 | Тач: XPT2046 | BLE HID: ESP32 BLE HID Combo
//  ВАЖНО: Tools → Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)"
// ============================================================

// ==================== LINUX ====================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>
#include <BleCombo.h>
#include <math.h>

// ============================================================
//  ДИСПЛЕЙ
// ============================================================
TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 44

// ============================================================
//  СЕНСОР
// ============================================================
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

#define TOUCH_MIN_X     300
#define TOUCH_MAX_X     3650
#define TOUCH_MIN_Y     250
#define TOUCH_MAX_Y     3700
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0
#define TOUCH_DEBOUNCE_MS 200

// ============================================================
//  ВЕБ-СЕРВЕР
// ============================================================
WebServer server(80);
const char* ap_ssid     = "Linux";
const char* ap_password = "12345678";

// ============================================================
//  ПАМЯТЬ
// ============================================================
Preferences prefs;
#define MAX_CHEAT_PAGES 20

// ============================================================
//  ТЕМЫ
// ============================================================
struct Theme {
  uint16_t bg, card, header, text, subtext, accent, accent2;
  const char* name;
};

uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

Theme themes[6] = {
  {0,0,0,0,0,0,0,"Dark"},
  {0,0,0,0,0,0,0,"Light"},
  {0,0,0,0,0,0,0,"Blue"},
  {0,0,0,0,0,0,0,"Red"},
  {0,0,0,0,0,0,0,"Green"},
  {0,0,0,0,0,0,0,"Purple"}
};
const int THEME_COUNT = 6;
int currentTheme = 0;

void buildThemes() {
  themes[0].bg = C(16,16,16);      themes[0].card = C(33,33,33);      themes[0].header = C(41,41,50);
  themes[0].text = C(255,255,255); themes[0].subtext = C(156,156,156);
  themes[0].accent = C(0,200,255); themes[0].accent2 = C(0,240,180);

  themes[1].bg = C(255,255,255);   themes[1].card = C(230,230,230);   themes[1].header = C(210,210,210);
  themes[1].text = C(0,0,0);       themes[1].subtext = C(66,66,66);
  themes[1].accent = C(0,60,220);  themes[1].accent2 = C(0,200,0);

  themes[2].bg = C(8,16,40);       themes[2].card = C(24,32,60);      themes[2].header = C(40,48,80);
  themes[2].text = C(255,255,255); themes[2].subtext = C(130,130,160);
  themes[2].accent = C(0,130,255); themes[2].accent2 = C(0,220,255);

  themes[3].bg = C(40,8,8);        themes[3].card = C(60,16,16);      themes[3].header = C(80,24,24);
  themes[3].text = C(255,255,255); themes[3].subtext = C(220,180,180);
  themes[3].accent = C(255,0,0);   themes[3].accent2 = C(255,80,0);

  themes[4].bg = C(8,32,16);       themes[4].card = C(24,48,32);      themes[4].header = C(32,60,40);
  themes[4].text = C(255,255,255); themes[4].subtext = C(160,200,160);
  themes[4].accent = C(0,200,0);   themes[4].accent2 = C(0,255,130);

  themes[5].bg = C(32,8,40);       themes[5].card = C(48,16,60);      themes[5].header = C(64,24,80);
  themes[5].text = C(255,255,255); themes[5].subtext = C(200,160,220);
  themes[5].accent = C(180,60,255); themes[5].accent2 = C(255,60,200);
}

// ============================================================
//  РЕЖИМЫ
// ============================================================
enum Mode {
  MODE_MENU,
  MODE_MULT,
  MODE_CALC,
  MODE_GAMES_MENU,
  MODE_GAME_TETRIS,
  MODE_GAME_FLAPPY,
  MODE_CHEAT,
  MODE_STOPWATCH,
  MODE_PC_MACRO,
  MODE_SETTINGS,
  MODE_THEME_SELECT,
  MODE_BG_COLOR_SELECT,
  MODE_MATRIX
};
Mode currentMode = MODE_MENU;

// ============================================================
//  МЕНЮ
// ============================================================
const char* menuItems[] = {
  "Multiplication", "Calculator", "Games", "Cheat sheets",
  "Stopwatch", "PC Macro", "Matrix", "Settings"
};
const int MENU_COUNT = 8;
const char* menuIcons[] = {"x", "+", "#", "S", "T", "P", "M", "*"};
int menuIndex = 0;
int pageIndex = 0;
int currentMultPage = 0;

#define MENU_CARD_H 29
#define MENU_GAP    3

// ============================================================
//  КАЛЬКУЛЯТОР
// ============================================================
String calcDisplay = "0";
String calcExpr = "";
float calcNum1 = 0, calcNum2 = 0, calcResult = 0;
char calcOp = 0;
int calcState = 0;
const int CALC_MAX_LEN = 12;

#define CALC_HISTORY_SIZE 5
String calcHistory[CALC_HISTORY_SIZE];
int calcHistoryCount = 0;
bool calcShowHistory = false;

const char* CALC_LABELS[20] = {
  "C", "+-", "%", "/",
  "7", "8", "9", "*",
  "4", "5", "6", "-",
  "1", "2", "3", "+",
  "r", "x2", "0", "="
};
const int CALC_COLS = 4;
const int CALC_ROWS = 5;
const int CALC_BTN_W = 54;
const int CALC_BTN_H = 36;
const int CALC_GAP   = 4;
const int CALC_START_X = 8;
const int CALC_START_Y = 112;

// ============================================================
//  СЕКУНДОМЕР / ТАЙМЕР
// ============================================================
enum StopwatchMode { SW_STOPWATCH, SW_TIMER };
struct Stopwatch {
  int mode;
  bool running;
  unsigned long startMs;
  unsigned long elapsedMs;
  unsigned long targetMs;
  int setHours;
  int setMinutes;
  int setSeconds;
  int selectedField;
  bool finished;
} sw;

// ============================================================
//  ОБОИ МЕНЮ
// ============================================================
uint16_t bgColors[8] = {
  0x0000, 0x0010, 0x0200, 0x4000,
  0x2104, 0x4010, 0x0240, 0x4200
};
const char* bgColorNames[8] = {
  "Theme", "Navy", "Forest", "Wine",
  "Charcoal", "Grape", "Teal", "Coffee"
};
int currentBgIndex = 0;

// ============================================================
//  ТЕТРИС
// ============================================================
struct Tetris {
  int board[12][20];
  int currentX, currentY;
  int currentBlock[4][4];
  int blockType, nextBlockType;
  bool over;
  int score;
  int level;
  int linesCleared;
  unsigned long lastFall;
  unsigned long overTime;
  int cellSize;
  int fallDelay;
  unsigned long lastRotate;
} tetris;

const int TETRIS_BLOCKS[7][4][4] = {
  {{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}},
  {{1,1},{1,1},{0,0},{0,0}},
  {{0,1,0},{1,1,1},{0,0,0},{0,0,0}},
  {{1,0,0},{1,1,1},{0,0,0},{0,0,0}},
  {{0,0,1},{1,1,1},{0,0,0},{0,0,0}},
  {{0,1,1},{1,1,0},{0,0,0},{0,0,0}},
  {{1,1,0},{0,1,1},{0,0,0},{0,0,0}}
};

// ============================================================
//  FLAPPY
// ============================================================
#define FLAPPY_PIPE_W   34
#define FLAPPY_GAP      78
#define FLAPPY_GRAVITY  0.35
#define FLAPPY_JUMP    -5.5
#define FLAPPY_SPEED    2
#define FLAPPY_BIRD_R   8

struct Flappy {
  float birdY;
  float birdVY;
  int birdX;
  int pipe1X, pipe1GapY;
  int pipe2X, pipe2GapY;
  bool over;
  int score;
  int prevScore;
  unsigned long lastFrame;
  unsigned long overTime;
  int groundY;
  bool isNight;
  int lastThemeChange;
  int birdColorIdx;
} flappy;

uint16_t birdBodyColors[4] = {0,0,0,0};
uint16_t birdWingColors[4] = {0,0,0,0};
uint16_t skyColors[2] = {0, 0};

// ============================================================
//  MATRIX
// ============================================================
#define MTX_COLS     30
#define MTX_CHAR_W   8
#define MTX_CHAR_H   10
#define MTX_ROW_COUNT (SCREEN_HEIGHT / MTX_CHAR_H)
int mtxHead[MTX_COLS];
int mtxLen[MTX_COLS];
unsigned long mtxLastTick = 0;
unsigned long mtxLastFrame = 0;

// ============================================================
//  PC MACRO
// ============================================================
#define MACRO_DELAY_SHORT 60
#define MACRO_DELAY_MED   350
#define MACRO_DELAY_LONG  700

// ============================================================
//  ТАЧ
// ============================================================
bool touchWasDown = false;
unsigned long lastTouchMs = 0;

bool readTouchClick(int &outX, int &outY) {
  bool isDown = ts.touched();
  if (isDown && !touchWasDown) {
    unsigned long now = millis();
    if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) { touchWasDown = true; return false; }
    TS_Point p = ts.getPoint();
    int x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    int y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
    if (TOUCH_INVERT_X) x = SCREEN_WIDTH - x;
    if (TOUCH_INVERT_Y) y = SCREEN_HEIGHT - y;
    outX = constrain(x, 0, SCREEN_WIDTH  - 1);
    outY = constrain(y, 0, SCREEN_HEIGHT - 1);
    lastTouchMs = now;
    touchWasDown = true;
    return true;
  }
  if (!isDown && touchWasDown) touchWasDown = false;
  return false;
}

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
void drawMenu();
void openSelected();
void drawMult();
void drawCalculator();
void drawGamesMenu();
void drawCheatPage();
void drawStopwatch();
void drawPcMacro();
void drawSettings();
void drawThemeSelect();
void drawBgColorSelect();
void restartTetris();
void restartFlappy();
void tetrisLoop();
void flappyLoop();
void stopwatchLoop();
void handleTouch(int x, int y);
void handleMenuTouch(int x, int y);
void handleCalcTouch(int x, int y);
void handleGamesMenuTouch(int x, int y);
void handleTetrisTouch(int x, int y);
void handleFlappyTouch(int x, int y);
void handlePageTouch(int x, int y);
void handleStopwatchTouch(int x, int y);
void handlePcMacroTouch(int x, int y);
void handleSettingsTouch(int x, int y);
void handleThemeTouch(int x, int y);
void handleBgColorTouch(int x, int y);
void handleMatrixTouch(int x, int y);

void setupWebServer();
void handleRoot();
void handleAPIPages();
void handleSave();
void handleClear();

void drawBootScreen();
void calcButtonPressed(int b);
void matrixInit();
void matrixTick();
void matrixDraw();
void drawHeader(const char* title, bool back);
void drawButton(int x, int y, int w, int h, uint16_t color, const char* label, uint16_t textColor);
uint16_t menuBg();
void runMacroCmd();
void runMacroLock();
void runMacroClose();

// ============================================================
//  ОБОИ
// ============================================================
uint16_t menuBg() {
  if (currentBgIndex == 0) return themes[currentTheme].bg;
  return bgColors[currentBgIndex];
}

// ============================================================
//  ГРАФИКА
// ============================================================
void drawSmoothCard(int x, int y, int w, int h, uint16_t color) {
  for (int i = 3; i >= 1; i--)
    tft.fillRoundRect(x + i, y + i, w, h, 10, C(8,8,8));
  tft.fillRoundRect(x, y, w, h, 10, color);
}

void drawHeader(const char* title, bool back) {
  Theme& th = themes[currentTheme];
  uint8_t r0 = (th.header >> 11) & 0x1F;
  uint8_t g0 = (th.header >> 5)  & 0x3F;
  uint8_t b0 =  th.header        & 0x1F;
  for (int i = 0; i < HEADER_HEIGHT; i++) {
    uint8_t r = map(i, 0, HEADER_HEIGHT, r0, min(31, r0 + 6));
    uint8_t g = map(i, 0, HEADER_HEIGHT, g0, min(63, g0 + 10));
    uint8_t b = map(i, 0, HEADER_HEIGHT, b0, min(31, b0 + 10));
    tft.fillRect(0, i, SCREEN_WIDTH, 1, (r << 11) | (g << 5) | b);
  }
  if (back) {
    tft.fillRoundRect(6, 6, 36, 32, 8, th.accent);
    tft.setTextColor(0x0000, th.accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(F("<"), 24, 22);
    tft.setTextColor(th.text, th.header);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.drawString(title, 50, HEADER_HEIGHT / 2);
    tft.setTextDatum(TL_DATUM);
  } else {
    tft.setTextColor(th.text, th.header);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.drawString(title, 12, HEADER_HEIGHT / 2);
    tft.setTextDatum(TL_DATUM);
  }
}

void drawButton(int x, int y, int w, int h, uint16_t color, const char* label, uint16_t textColor) {
  drawSmoothCard(x, y, w, h, color);
  tft.setTextColor(textColor, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString(label, x + w / 2, y + h / 2);
  tft.setTextDatum(TL_DATUM);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  tft.init();
  tft.setRotation(0);
  buildThemes();
  tft.fillScreen(themes[currentTheme].bg);

  birdBodyColors[0] = C(255, 220, 0);
  birdBodyColors[1] = C(255, 80, 80);
  birdBodyColors[2] = C(80, 220, 80);
  birdBodyColors[3] = C(180, 100, 255);
  birdWingColors[0] = C(255, 255, 200);
  birdWingColors[1] = C(255, 200, 200);
  birdWingColors[2] = C(200, 255, 200);
  birdWingColors[3] = C(220, 200, 255);

  skyColors[0] = C(16, 16, 16);
  skyColors[1] = C(10, 10, 30);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

  prefs.begin("settings", true);
  currentTheme  = prefs.getInt("theme", 0);
  currentBgIndex= prefs.getInt("bg", 0);
  prefs.end();
  if (currentTheme   < 0 || currentTheme   >= THEME_COUNT) currentTheme   = 0;
  if (currentBgIndex < 0 || currentBgIndex >= 8)           currentBgIndex = 0;

  WiFi.softAP(ap_ssid, ap_password);
  setupWebServer();

  // bleDevice.begin() защищён (protected), используем публичный Keyboard.begin()
  Keyboard.begin();
  Serial.println(F("BLE HID started. Device name: ESP32 BLE Combo"));

  drawBootScreen();
  delay(200);
  drawMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  server.handleClient();

  int tx, ty;
  if (readTouchClick(tx, ty)) handleTouch(tx, ty);

  switch (currentMode) {
    case MODE_GAME_TETRIS: tetrisLoop(); break;
    case MODE_GAME_FLAPPY: flappyLoop(); break;
    case MODE_STOPWATCH:   stopwatchLoop(); break;
    case MODE_MATRIX: {
      matrixTick();
      if (millis() - mtxLastFrame > 50) {
        mtxLastFrame = millis();
        matrixDraw();
      }
      break;
    }
    case MODE_PC_MACRO: {
      static unsigned long lastCheck = 0;
      static bool lastState = false;
      if (millis() - lastCheck > 1000) {
        lastCheck = millis();
        bool now = bleDevice.isConnected();
        if (now != lastState) {
          lastState = now;
          drawPcMacro();
        }
      }
      break;
    }
    default: break;
  }
}

// ============================================================
//  BOOT
// ============================================================
void drawBootScreen() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  for (int i = 0; i < 80; i++) {
    uint16_t c = C(random(40, 220), random(40, 220), random(40, 220));
    tft.fillCircle(random(SCREEN_WIDTH), random(SCREEN_HEIGHT), random(1, 4), c);
  }
  delay(200);
  tft.fillScreen(th.bg);

  tft.drawRoundRect(20, 110, 200, 70, 15, th.accent);
  tft.drawRoundRect(23, 113, 194, 64, 12, th.accent2);
  tft.setTextColor(th.accent, th.bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.drawString(F("Linux"), SCREEN_WIDTH / 2, 145);
  tft.setTextDatum(TL_DATUM);

  for (int i = 5; i <= 200; i += 5) {
    tft.fillRoundRect(20, 210, i, 8, 4, th.accent);
    delay(8);
  }
  delay(200);
}

// ============================================================
//  DISPATCH
// ============================================================
void handleTouch(int x, int y) {
  switch (currentMode) {
    case MODE_MENU:            handleMenuTouch(x, y); break;
    case MODE_MULT:
      if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); }
      else if (y > SCREEN_HEIGHT - 40 && y < SCREEN_HEIGHT - 8) {
        if      (x > 8   && x < 116) { if (currentMultPage > 0) currentMultPage--; drawMult(); }
        else if (x > 124 && x < 232) { if (currentMultPage < 8) currentMultPage++; drawMult(); }
      }
      break;
    case MODE_CALC:            handleCalcTouch(x, y); break;
    case MODE_GAMES_MENU:      handleGamesMenuTouch(x, y); break;
    case MODE_GAME_TETRIS:     handleTetrisTouch(x, y); break;
    case MODE_GAME_FLAPPY:     handleFlappyTouch(x, y); break;
    case MODE_CHEAT:           handlePageTouch(x, y); break;
    case MODE_STOPWATCH:       handleStopwatchTouch(x, y); break;
    case MODE_PC_MACRO:        handlePcMacroTouch(x, y); break;
    case MODE_SETTINGS:        handleSettingsTouch(x, y); break;
    case MODE_THEME_SELECT:    handleThemeTouch(x, y); break;
    case MODE_BG_COLOR_SELECT: handleBgColorTouch(x, y); break;
    case MODE_MATRIX:          handleMatrixTouch(x, y); break;
    default: break;
  }
}

// ============================================================
//  МЕНЮ
// ============================================================
void drawMenu() {
  Theme& th = themes[currentTheme];
  uint16_t bg = menuBg();
  tft.fillScreen(bg);
  drawHeader("Linux", false);

  const int cardX = 8;
  const int cardW = SCREEN_WIDTH - 16;

  uint16_t iconColors[8] = {
    C(0,200,255), C(0,220,100), C(255,160,0),
    C(180,100,255), C(255,80,200), C(255,60,60), C(0,255,100), C(156,156,156)
  };

  int y = HEADER_HEIGHT + 4;
  for (int i = 0; i < MENU_COUNT; i++) {
    tft.fillRoundRect(cardX, y, cardW, MENU_CARD_H, 8, th.card);
    tft.fillCircle(cardX + 16, y + MENU_CARD_H / 2, 10, iconColors[i]);
    tft.setTextColor(0x0000, iconColors[i]);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString(menuIcons[i], cardX + 16, y + MENU_CARD_H / 2);
    tft.setTextColor(th.text, th.card);
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.drawString(menuItems[i], cardX + 34, y + MENU_CARD_H / 2);
    tft.setTextColor(th.accent, th.card);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(F(">"), cardX + cardW - 8, y + MENU_CARD_H / 2);
    tft.setTextDatum(TL_DATUM);
    y += MENU_CARD_H + MENU_GAP;
  }
}

void handleMenuTouch(int x, int y) {
  const int cardX = 8;
  const int cardW = SCREEN_WIDTH - 16;
  int itemY = HEADER_HEIGHT + 4;
  for (int i = 0; i < MENU_COUNT; i++) {
    if (x > cardX && x < cardX + cardW && y > itemY && y < itemY + MENU_CARD_H) {
      menuIndex = i;
      openSelected();
      return;
    }
    itemY += MENU_CARD_H + MENU_GAP;
  }
}

void openSelected() {
  switch (menuIndex) {
    case 0: currentMode = MODE_MULT; currentMultPage = 0; drawMult(); break;
    case 1: currentMode = MODE_CALC; drawCalculator(); break;
    case 2: currentMode = MODE_GAMES_MENU; drawGamesMenu(); break;
    case 3: currentMode = MODE_CHEAT; pageIndex = 0; drawCheatPage(); break;
    case 4:
      currentMode = MODE_STOPWATCH;
      sw.mode = SW_STOPWATCH;
      sw.running = false;
      sw.elapsedMs = 0;
      sw.setHours = 0; sw.setMinutes = 1; sw.setSeconds = 0;
      sw.selectedField = 1;
      sw.finished = false;
      sw.targetMs = 60000;
      drawStopwatch();
      break;
    case 5: currentMode = MODE_PC_MACRO; drawPcMacro(); break;
    case 6: currentMode = MODE_MATRIX; matrixInit(); break;
    case 7: currentMode = MODE_SETTINGS; drawSettings(); break;
  }
}

// ============================================================
//  MULTIPLICATION
// ============================================================
void drawMult() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  char title[24];
  snprintf(title, sizeof(title), "Table x %d", currentMultPage + 1);
  drawHeader(title, true);

  int startY = HEADER_HEIGHT + 10;
  int cardW = 70, cardH = 56, gapX = 6, gapY = 6;
  int totalW = 3 * cardW + 2 * gapX;
  int startX = (SCREEN_WIDTH - totalW) / 2;
  int n = currentMultPage + 1;

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int i = row * 3 + col;
      int bx = startX + col * (cardW + gapX);
      int by = startY + row * (cardH + gapY);
      tft.fillRoundRect(bx, by, cardW, cardH, 10, th.card);
      char line1[16], line2[16];
      snprintf(line1, sizeof(line1), "%d x %d", n, i + 1);
      snprintf(line2, sizeof(line2), "= %d", n * (i + 1));
      tft.setTextColor(th.subtext, th.card);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(1);
      tft.drawString(line1, bx + cardW / 2, by + 16);
      tft.setTextColor(th.accent, th.card);
      tft.setTextSize(2);
      tft.drawString(line2, bx + cardW / 2, by + 38);
      tft.setTextDatum(TL_DATUM);
    }
  }

  tft.setTextColor(th.subtext, th.bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  char pg[16];
  snprintf(pg, sizeof(pg), "%d / 9", currentMultPage + 1);
  tft.drawString(pg, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 48);

  drawButton(8, SCREEN_HEIGHT - 40, 108, 32, th.header, "< Back", th.text);
  drawButton(124, SCREEN_HEIGHT - 40, 108, 32, th.accent, "Next >", 0x0000);
}

// ============================================================
//  КАЛЬКУЛЯТОР
// ============================================================
String formatResult(float v) {
  if (isnan(v) || isinf(v)) return "Err";
  if (fabs(v - round(v)) < 0.0001) return String((long)round(v));
  return String(v, 4);
}

void calcPushHistory(String entry) {
  if (calcHistoryCount < CALC_HISTORY_SIZE) {
    calcHistory[calcHistoryCount++] = entry;
  } else {
    for (int i = 0; i < CALC_HISTORY_SIZE - 1; i++) calcHistory[i] = calcHistory[i + 1];
    calcHistory[CALC_HISTORY_SIZE - 1] = entry;
  }
}

void drawCalculator() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Calculator", true);

  const int cardX = 10, cardY = 50, cardW = SCREEN_WIDTH - 20, cardH = 54;
  drawSmoothCard(cardX, cardY, cardW, cardH, th.card);

  const int rightX = cardX + cardW - 10;
  tft.setTextColor(th.subtext, th.card);
  tft.setTextDatum(TR_DATUM);
  tft.setTextSize(1);
  tft.drawString(calcExpr, rightX, cardY + 10);

  tft.setTextColor(th.accent, th.card);
  tft.setTextDatum(MR_DATUM);
  tft.setTextSize(2);
  tft.drawString(calcDisplay, rightX, cardY + 34);
  tft.setTextDatum(TL_DATUM);

  for (int row = 0; row < CALC_ROWS; row++) {
    for (int col = 0; col < CALC_COLS; col++) {
      int bx = CALC_START_X + col * (CALC_BTN_W + CALC_GAP);
      int by = CALC_START_Y + row * (CALC_BTN_H + CALC_GAP);
      int idx = row * CALC_COLS + col;
      const char* lbl = CALC_LABELS[idx];
      uint16_t c;
      if      (lbl[0] == '=')                        c = C(0,220,100);
      else if (lbl[0] == 'C')                        c = C(255,60,60);
      else if (lbl[0] == '+' && lbl[1] == '-')       c = C(120,120,220);
      else if (strchr("+-*/", lbl[0]) && lbl[1]==0)  c = C(255,160,0);
      else if (lbl[0] == '%' || lbl[0] == 'r' || lbl[0] == 'x') c = C(120,120,220);
      else                                           c = th.card;

      drawSmoothCard(bx, by, CALC_BTN_W, CALC_BTN_H, c);
      tft.setTextColor(0x0000, c);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.drawString(lbl, bx + CALC_BTN_W / 2, by + CALC_BTN_H / 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  tft.setTextColor(th.subtext, th.bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  char hist[40];
  snprintf(hist, sizeof(hist), "History: %d  (tap here)", calcHistoryCount);
  tft.drawString(hist, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 6);
  tft.setTextDatum(TL_DATUM);
}

void handleCalcTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }

  if (y > SCREEN_HEIGHT - 14 && y < SCREEN_HEIGHT) {
    calcShowHistory = !calcShowHistory;
    Theme& th = themes[currentTheme];
    if (calcShowHistory) {
      tft.fillScreen(th.bg);
      drawHeader("History", true);
      int yy = HEADER_HEIGHT + 10;
      if (calcHistoryCount == 0) {
        tft.setTextColor(th.subtext, th.bg);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.drawString(F("No history yet"), SCREEN_WIDTH / 2, 150);
        tft.setTextDatum(TL_DATUM);
      } else {
        for (int i = 0; i < calcHistoryCount; i++) {
          drawSmoothCard(8, yy, SCREEN_WIDTH - 16, 26, th.card);
          tft.setTextColor(th.text, th.card);
          tft.setTextSize(1);
          tft.setCursor(18, yy + 9);
          tft.print(calcHistory[i]);
          yy += 30;
        }
      }
    } else {
      drawCalculator();
    }
    return;
  }

  if (calcShowHistory) { calcShowHistory = false; drawCalculator(); return; }

  for (int row = 0; row < CALC_ROWS; row++) {
    for (int col = 0; col < CALC_COLS; col++) {
      int bx = CALC_START_X + col * (CALC_BTN_W + CALC_GAP);
      int by = CALC_START_Y + row * (CALC_BTN_H + CALC_GAP);
      if (x > bx && x < bx + CALC_BTN_W && y > by && y < by + CALC_BTN_H) {
        calcButtonPressed(row * CALC_COLS + col);
        return;
      }
    }
  }
}

void calcButtonPressed(int b) {
  String label = CALC_LABELS[b];

  if (label == "C") {
    calcDisplay = "0"; calcExpr = "";
    calcNum1 = 0; calcNum2 = 0; calcOp = 0; calcState = 0;
  } else if (label == "+-") {
    if (calcDisplay.length() > 0 && calcDisplay != "0") {
      if (calcDisplay[0] == '-') calcDisplay = calcDisplay.substring(1);
      else                       calcDisplay = "-" + calcDisplay;
    }
  } else if (label == "%") {
    float v = calcDisplay.toFloat() / 100.0f;
    calcDisplay = formatResult(v);
    calcState = 1;
  } else if (label == "r") {
    float v = calcDisplay.toFloat();
    calcDisplay = (v < 0) ? "Err" : formatResult(sqrtf(v));
    calcState = 1;
  } else if (label == "x2") {
    float v = calcDisplay.toFloat();
    calcDisplay = formatResult(v * v);
    calcState = 1;
  } else if (label == "=") {
    if (calcState == 1 || calcState == 2) {
      if (calcState == 1) calcNum2 = calcDisplay.toFloat();
      switch (calcOp) {
        case '+': calcResult = calcNum1 + calcNum2; break;
        case '-': calcResult = calcNum1 - calcNum2; break;
        case '*': calcResult = calcNum1 * calcNum2; break;
        case '/': calcResult = (calcNum2 != 0) ? calcNum1 / calcNum2 : 0; break;
        default:  calcResult = calcDisplay.toFloat(); break;
      }
      String histLine = formatResult(calcNum1);
      if (calcOp) { histLine += " "; histLine += calcOp; histLine += " "; }
      histLine += formatResult(calcNum2);
      histLine += " = ";
      histLine += formatResult(calcResult);
      calcPushHistory(histLine);

      calcExpr = formatResult(calcNum1);
      if (calcOp) { calcExpr += " "; calcExpr += calcOp; calcExpr += " "; }
      calcExpr += formatResult(calcNum2);
      calcExpr += " =";
      calcDisplay = formatResult(calcResult);
      calcNum1 = calcResult;
      calcNum2 = 0;
      calcState = 3;
    }
  } else if (label == "+" || label == "-" || label == "*" || label == "/") {
    if      (calcState == 3) { calcNum1 = calcDisplay.toFloat(); calcExpr = ""; }
    else if (calcState == 1) calcNum1 = calcDisplay.toFloat();
    else if (calcState == 0) calcNum1 = calcDisplay.toFloat();
    calcOp = label[0];
    calcExpr = formatResult(calcNum1) + " " + label + " ";
    calcDisplay = "0";
    calcState = 2;
  } else {
    if (calcState == 3) { calcDisplay = "0"; calcExpr = ""; calcState = 0; }
    if (calcDisplay == "0" || calcState == 2) {
      calcDisplay = label;
      if (calcState != 2) calcExpr = "";
      calcState = 1;
    } else {
      if ((int)calcDisplay.length() < CALC_MAX_LEN) calcDisplay += label;
      calcState = 1;
    }
  }
  if (!calcShowHistory) drawCalculator();
}

// ============================================================
//  МЕНЮ ИГР
// ============================================================
void drawGamesMenu() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Games", true);

  const char* games[] = {"Tetris", "Flappy Bird"};
  const char* subs[]  = {"Levels", "Day/Night"};
  int startY = HEADER_HEIGHT + 20;
  for (int i = 0; i < 2; i++) {
    int y = startY + i * 84;
    drawSmoothCard(16, y, SCREEN_WIDTH - 32, 74, th.card);
    tft.setTextColor(th.accent, th.card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(games[i], SCREEN_WIDTH / 2, y + 26);
    tft.setTextColor(th.subtext, th.card);
    tft.setTextSize(1);
    tft.drawString(subs[i], SCREEN_WIDTH / 2, y + 54);
    tft.setTextDatum(TL_DATUM);
  }
}

void handleGamesMenuTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }
  int startY = HEADER_HEIGHT + 20;
  for (int i = 0; i < 2; i++) {
    int gy = startY + i * 84;
    if (x > 16 && x < SCREEN_WIDTH - 16 && y > gy && y < gy + 74) {
      if      (i == 0) { currentMode = MODE_GAME_TETRIS; restartTetris(); }
      else if (i == 1) { currentMode = MODE_GAME_FLAPPY; restartFlappy(); }
      return;
    }
  }
}

// ============================================================
//  ТЕТРИС
// ============================================================
uint16_t tetrisColor(int t) {
  switch (t) {
    case 0: return C(0, 200, 255);
    case 1: return C(255, 220, 0);
    case 2: return C(200, 0, 220);
    case 3: return C(0, 220, 0);
    case 4: return C(255, 0, 0);
    case 5: return C(255, 140, 0);
    case 6: return C(120, 60, 220);
  }
  return C(255, 255, 255);
}

bool tetrisCollides(int x, int y, int block[4][4]) {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (block[i][j]) {
        int bx = x + j, by = y + i;
        if (bx < 0 || bx >= 12 || by >= 20) return true;
        if (by >= 0 && tetris.board[bx][by]) return true;
      }
  return false;
}

void tetrisSpawn() {
  tetris.currentX = 4; tetris.currentY = 0;
  tetris.blockType = tetris.nextBlockType;
  tetris.nextBlockType = random(0, 7);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tetris.currentBlock[i][j] = TETRIS_BLOCKS[tetris.blockType][i][j];
  if (tetrisCollides(tetris.currentX, tetris.currentY, tetris.currentBlock)) {
    tetris.over = true;
    tetris.overTime = millis();
  }
}

void tetrisLock() {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (tetris.currentBlock[i][j]) {
        int bx = tetris.currentX + j, by = tetris.currentY + i;
        if (bx >= 0 && bx < 12 && by >= 0 && by < 20)
          tetris.board[bx][by] = tetris.blockType + 1;
      }
  int cleared = 0;
  for (int y = 19; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < 12; x++) if (!tetris.board[x][y]) { full = false; break; }
    if (full) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < 12; x++)
          tetris.board[x][yy] = tetris.board[x][yy - 1];
      for (int x = 0; x < 12; x++) tetris.board[x][0] = 0;
      cleared++;
      y++;
    }
  }
  if (cleared > 0) {
    tetris.linesCleared += cleared;
    tetris.score += 10 * cleared * cleared;
    int newLevel = 1 + tetris.linesCleared / 5;
    if (newLevel > 10) newLevel = 10;
    if (newLevel != tetris.level) {
      tetris.level = newLevel;
      tetris.fallDelay = 500 - (tetris.level - 1) * 40;
      if (tetris.fallDelay < 100) tetris.fallDelay = 100;
    }
  }
  tetrisSpawn();
}

void drawTetrisHud() {
  Theme& th = themes[currentTheme];
  tft.fillRect(SCREEN_WIDTH - 90, 8, 90, 28, th.header);
  tft.setTextColor(th.text, th.header);
  tft.setTextSize(1);
  tft.setTextDatum(TR_DATUM);
  char buf[32];
  snprintf(buf, sizeof(buf), "Lv%d  %d", tetris.level, tetris.score);
  tft.drawString(buf, SCREEN_WIDTH - 8, 16);
  tft.setTextDatum(TL_DATUM);
}

void restartTetris() {
  tetris.over = false;
  tetris.score = 0;
  tetris.level = 1;
  tetris.linesCleared = 0;
  tetris.fallDelay = 500;
  tetris.cellSize = 11;
  tetris.overTime = 0;
  for (int x = 0; x < 12; x++)
    for (int y = 0; y < 20; y++)
      tetris.board[x][y] = 0;
  tetris.nextBlockType = random(0, 7);
  tetrisSpawn();
  tetris.lastFall = millis();

  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Tetris", true);
  drawTetrisHud();

  int boardW = 12 * tetris.cellSize;
  int boardH = 20 * tetris.cellSize;
  int offsetX = (SCREEN_WIDTH - boardW) / 2;
  int offsetY = HEADER_HEIGHT + 6;
  tft.drawRect(offsetX - 2, offsetY - 2, boardW + 4, boardH + 4, th.subtext);
}

void tetrisLoop() {
  if (millis() - tetris.lastFall < (unsigned long)tetris.fallDelay) return;
  tetris.lastFall = millis();
  Theme& th = themes[currentTheme];

  if (!tetris.over) {
    if (!tetrisCollides(tetris.currentX, tetris.currentY + 1, tetris.currentBlock))
      tetris.currentY++;
    else
      tetrisLock();
  }

  tft.fillScreen(th.bg);
  drawHeader("Tetris", true);
  drawTetrisHud();

  int boardW = 12 * tetris.cellSize;
  int boardH = 20 * tetris.cellSize;
  int offsetX = (SCREEN_WIDTH - boardW) / 2;
  int offsetY = HEADER_HEIGHT + 6;

  tft.drawRect(offsetX - 2, offsetY - 2, boardW + 4, boardH + 4, th.subtext);

  for (int x = 0; x < 12; x++)
    for (int y = 0; y < 20; y++)
      if (tetris.board[x][y])
        tft.fillRect(offsetX + x * tetris.cellSize, offsetY + y * tetris.cellSize,
                     tetris.cellSize - 1, tetris.cellSize - 1,
                     tetrisColor(tetris.board[x][y] - 1));

  if (!tetris.over) {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        if (tetris.currentBlock[i][j]) {
          int bx = tetris.currentX + j, by = tetris.currentY + i;
          if (by >= 0)
            tft.fillRect(offsetX + bx * tetris.cellSize, offsetY + by * tetris.cellSize,
                         tetris.cellSize - 1, tetris.cellSize - 1,
                         tetrisColor(tetris.blockType));
        }
  }

  if (tetris.over) {
    tft.fillRoundRect(50, 140, 140, 70, 15, th.card);
    tft.drawRoundRect(50, 140, 140, 70, 15, C(255, 0, 0));
    tft.setTextColor(C(255, 0, 0), th.card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(F("GAME"), 120, 155);
    tft.drawString(F("OVER"), 120, 178);
    tft.setTextSize(1);
    tft.setTextColor(th.text, th.card);
    tft.drawString(F("Tap = restart"), 120, 198);
    tft.setTextDatum(TL_DATUM);
  }
}

void handleTetrisTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_GAMES_MENU; drawGamesMenu(); return; }
  if (tetris.over) {
    if (millis() - tetris.overTime > 600) restartTetris();
    return;
  }

  if (y < 70) {
    if (millis() - tetris.lastRotate < 150) return;
    tetris.lastRotate = millis();
    int rotated[4][4];
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        rotated[j][3 - i] = tetris.currentBlock[i][j];
    if (!tetrisCollides(tetris.currentX, tetris.currentY, rotated)) {
      for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
          tetris.currentBlock[i][j] = rotated[i][j];
    }
  } else if (y > 290) {
    while (!tetrisCollides(tetris.currentX, tetris.currentY + 1, tetris.currentBlock))
      tetris.currentY++;
    tetrisLock();
  } else {
    if (x < SCREEN_WIDTH / 2) {
      if (!tetrisCollides(tetris.currentX - 1, tetris.currentY, tetris.currentBlock))
        tetris.currentX--;
    } else {
      if (!tetrisCollides(tetris.currentX + 1, tetris.currentY, tetris.currentBlock))
        tetris.currentX++;
    }
  }
}

// ============================================================
//  FLAPPY
// ============================================================
uint16_t flappySky() {
  return flappy.isNight ? skyColors[1] : skyColors[0];
}

void drawSky() {
  tft.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, flappy.groundY - HEADER_HEIGHT, flappySky());
}

void drawPipeFull(int x, int gapY) {
  uint16_t pipeColor = flappy.isNight ? C(0, 120, 30) : C(0, 180, 40);
  uint16_t pipeDark  = C(0, 100, 20);
  uint16_t pipeLight = flappy.isNight ? C(80, 200, 100) : C(120, 255, 140);

  int topPipeBottom = gapY;
  int bottomPipeTop = gapY + FLAPPY_GAP;
  int groundY = flappy.groundY;

  if (topPipeBottom > HEADER_HEIGHT) {
    tft.fillRect(x, HEADER_HEIGHT, FLAPPY_PIPE_W, topPipeBottom - HEADER_HEIGHT, pipeColor);
    tft.drawRect(x, HEADER_HEIGHT, FLAPPY_PIPE_W, topPipeBottom - HEADER_HEIGHT, pipeDark);
    tft.drawFastVLine(x + 4, HEADER_HEIGHT, topPipeBottom - HEADER_HEIGHT, pipeLight);
    int capY = topPipeBottom - 8;
    tft.fillRect(x - 2, capY, FLAPPY_PIPE_W + 4, 8, pipeColor);
    tft.drawRect(x - 2, capY, FLAPPY_PIPE_W + 4, 8, pipeDark);
  }
  if (bottomPipeTop < groundY) {
    int h = groundY - bottomPipeTop;
    tft.fillRect(x, bottomPipeTop, FLAPPY_PIPE_W, h, pipeColor);
    tft.drawRect(x, bottomPipeTop, FLAPPY_PIPE_W, h, pipeDark);
    tft.drawFastVLine(x + 4, bottomPipeTop, h, pipeLight);
    tft.fillRect(x - 2, bottomPipeTop, FLAPPY_PIPE_W + 4, 8, pipeColor);
    tft.drawRect(x - 2, bottomPipeTop, FLAPPY_PIPE_W + 4, 8, pipeDark);
  }
}

void shiftPipe(int oldX, int newX, int gapY) {
  uint16_t pipeColor = flappy.isNight ? C(0, 120, 30) : C(0, 180, 40);
  uint16_t sky = flappySky();

  int topPipeBottom = gapY;
  int bottomPipeTop = gapY + FLAPPY_GAP;
  int groundY = flappy.groundY;

  int eraseX = oldX + FLAPPY_PIPE_W;
  if (eraseX >= 0 && eraseX < SCREEN_WIDTH) {
    if (topPipeBottom > HEADER_HEIGHT)
      tft.fillRect(eraseX, HEADER_HEIGHT, 2, topPipeBottom - HEADER_HEIGHT, sky);
    if (bottomPipeTop < groundY)
      tft.fillRect(eraseX, bottomPipeTop, 2, groundY - bottomPipeTop, sky);
  }

  if (newX >= 0 && newX < SCREEN_WIDTH) {
    if (topPipeBottom > HEADER_HEIGHT)
      tft.fillRect(newX, HEADER_HEIGHT, 2, topPipeBottom - HEADER_HEIGHT, pipeColor);
    if (bottomPipeTop < groundY)
      tft.fillRect(newX, bottomPipeTop, 2, groundY - bottomPipeTop, pipeColor);
  }
}

void drawBird(int x, int y) {
  uint16_t body  = birdBodyColors[flappy.birdColorIdx];
  uint16_t wing  = birdWingColors[flappy.birdColorIdx];
  uint16_t eye   = C(255, 255, 255);
  uint16_t pupil = C(0, 0, 0);
  uint16_t beak  = C(255, 120, 0);

  tft.fillCircle(x, y, FLAPPY_BIRD_R, body);
  tft.fillCircle(x - 2, y + 1, 4, wing);
  tft.fillCircle(x + 3, y - 2, 3, eye);
  tft.fillCircle(x + 4, y - 2, 1, pupil);
  tft.fillTriangle(x + 6, y, x + 12, y + 2, x + 6, y + 4, beak);
}

void eraseBird(int x, int y) {
  uint16_t sky = flappySky();
  int r = FLAPPY_BIRD_R + 3;
  int ex = x - r - 3;
  int ey = y - r;
  int ew = 2 * r + 8;
  int eh = 2 * r;
  if (ey < HEADER_HEIGHT) { eh -= (HEADER_HEIGHT - ey); ey = HEADER_HEIGHT; }
  if (ey + eh > flappy.groundY) eh = flappy.groundY - ey;
  if (eh <= 0) return;
  tft.fillRect(ex, ey, ew, eh, sky);
}

void restartFlappy() {
  flappy.birdX = 70;
  flappy.birdY = SCREEN_HEIGHT / 2;
  flappy.birdVY = 0;
  flappy.pipe1X = SCREEN_WIDTH + 20;
  flappy.pipe2X = SCREEN_WIDTH + 20 + 130;
  flappy.pipe1GapY = HEADER_HEIGHT + 40 + random(0, 60);
  flappy.pipe2GapY = HEADER_HEIGHT + 40 + random(0, 60);
  flappy.over = false;
  flappy.score = 0;
  flappy.prevScore = 0;
  flappy.overTime = 0;
  flappy.groundY = SCREEN_HEIGHT - 30;
  flappy.lastFrame = millis();
  flappy.isNight = false;
  flappy.lastThemeChange = 0;
  flappy.birdColorIdx = 0;

  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Flappy", true);
  drawSky();

  tft.fillRect(0, flappy.groundY, SCREEN_WIDTH, SCREEN_HEIGHT - flappy.groundY, C(80, 50, 20));
  tft.drawFastHLine(0, flappy.groundY, SCREEN_WIDTH, C(120, 80, 30));

  drawPipeFull(flappy.pipe1X, flappy.pipe1GapY);
  drawPipeFull(flappy.pipe2X, flappy.pipe2GapY);
  drawBird(flappy.birdX, (int)flappy.birdY);

  tft.setTextColor(th.text, th.header);
  tft.setTextSize(1);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(F("Score: 0"), SCREEN_WIDTH - 8, 16);
  tft.setTextDatum(TL_DATUM);
}

void flappyLoop() {
  if (flappy.over) return;
  if (millis() - flappy.lastFrame < 33) return;
  flappy.lastFrame = millis();

  Theme& th = themes[currentTheme];

  int prevBirdY = (int)flappy.birdY;
  int prevP1X = flappy.pipe1X;
  int prevP2X = flappy.pipe2X;

  flappy.birdVY += FLAPPY_GRAVITY;
  flappy.birdY  += flappy.birdVY;

  if (flappy.birdY < HEADER_HEIGHT + 14) {
    flappy.birdY = HEADER_HEIGHT + 14;
    flappy.birdVY = 0;
  }
  if (flappy.birdY > flappy.groundY - 14) {
    flappy.birdY = flappy.groundY - 14;
    flappy.over = true;
    flappy.overTime = millis();
  }

  flappy.pipe1X -= FLAPPY_SPEED;
  flappy.pipe2X -= FLAPPY_SPEED;

  if (flappy.pipe1X < -FLAPPY_PIPE_W - 10) {
    flappy.pipe1X = SCREEN_WIDTH + 20;
    flappy.pipe1GapY = HEADER_HEIGHT + 40 + random(0, 60);
    flappy.score++;
  }
  if (flappy.pipe2X < -FLAPPY_PIPE_W - 10) {
    flappy.pipe2X = SCREEN_WIDTH + 20;
    flappy.pipe2GapY = HEADER_HEIGHT + 40 + random(0, 60);
    flappy.score++;
  }

  if (flappy.score / 10 != flappy.lastThemeChange) {
    flappy.lastThemeChange = flappy.score / 10;
    flappy.isNight = !flappy.isNight;
    flappy.birdColorIdx = (flappy.birdColorIdx + 1) % 4;
    drawSky();
    tft.fillRect(0, flappy.groundY, SCREEN_WIDTH, SCREEN_HEIGHT - flappy.groundY, C(80, 50, 20));
    tft.drawFastHLine(0, flappy.groundY, SCREEN_WIDTH, C(120, 80, 30));
    drawPipeFull(flappy.pipe1X, flappy.pipe1GapY);
    drawPipeFull(flappy.pipe2X, flappy.pipe2GapY);
    drawBird(flappy.birdX, (int)flappy.birdY);
  }

  int bx = flappy.birdX;
  int by = (int)flappy.birdY;

  for (int p = 0; p < 2 && !flappy.over; p++) {
    int px = (p == 0) ? flappy.pipe1X : flappy.pipe2X;
    int py = (p == 0) ? flappy.pipe1GapY : flappy.pipe2GapY;
    if (bx + FLAPPY_BIRD_R > px && bx - FLAPPY_BIRD_R < px + FLAPPY_PIPE_W) {
      if (by - FLAPPY_BIRD_R < py || by + FLAPPY_BIRD_R > py + FLAPPY_GAP) {
        flappy.over = true;
        flappy.overTime = millis();
      }
    }
  }

  eraseBird(bx, prevBirdY);

  if (prevP1X != flappy.pipe1X) shiftPipe(prevP1X, flappy.pipe1X, flappy.pipe1GapY);
  if (prevP2X != flappy.pipe2X) shiftPipe(prevP2X, flappy.pipe2X, flappy.pipe2GapY);

  drawBird(bx, by);

  if (flappy.score != flappy.prevScore) {
    flappy.prevScore = flappy.score;
    tft.fillRect(SCREEN_WIDTH - 90, 8, 90, 24, th.header);
    tft.setTextColor(th.text, th.header);
    tft.setTextSize(1);
    tft.setTextDatum(TR_DATUM);
    char scoreBuf[24];
    snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", flappy.score);
    tft.drawString(scoreBuf, SCREEN_WIDTH - 8, 16);
    tft.setTextDatum(TL_DATUM);
  }

  if (flappy.over) {
    tft.fillRoundRect(50, 140, 140, 70, 15, th.card);
    tft.drawRoundRect(50, 140, 140, 70, 15, C(255, 0, 0));
    tft.setTextColor(C(255, 0, 0), th.card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(F("GAME"), 120, 155);
    tft.drawString(F("OVER"), 120, 178);
    tft.setTextSize(1);
    tft.setTextColor(th.text, th.card);
    tft.drawString(F("Tap = restart"), 120, 198);
    tft.setTextDatum(TL_DATUM);
  }
}

void handleFlappyTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_GAMES_MENU; drawGamesMenu(); return; }
  if (flappy.over) {
    if (millis() - flappy.overTime > 600) restartFlappy();
    return;
  }
  flappy.birdVY = FLAPPY_JUMP;
}
/ ============================================================
//  СЕКУНДОМЕР / ТАЙМЕР
// ============================================================
void formatTime(unsigned long ms, char* out) {
  unsigned long total = ms / 1000;
  unsigned long h  = total / 3600;
  unsigned long m  = (total % 3600) / 60;
  unsigned long s  = total % 60;
  unsigned long cs = (ms % 1000) / 10;
  sprintf(out, "%02lu:%02lu:%02lu.%02lu", h, m, s, cs);
}

void drawStopwatch() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader(sw.mode == SW_STOPWATCH ? "Stopwatch" : "Timer", true);

  unsigned long displayMs;
  if (sw.mode == SW_STOPWATCH) {
    displayMs = sw.running ? (millis() - sw.startMs + sw.elapsedMs) : sw.elapsedMs;
  } else {
    if (sw.running) {
      unsigned long elapsed = millis() - sw.startMs + sw.elapsedMs;
      displayMs = (elapsed < sw.targetMs) ? (sw.targetMs - elapsed) : 0;
    } else {
      displayMs = (sw.elapsedMs < sw.targetMs) ? (sw.targetMs - sw.elapsedMs) : sw.targetMs;
    }
  }

  drawSmoothCard(10, 60, SCREEN_WIDTH - 20, 80, th.card);
  char buf[24];
  formatTime(displayMs, buf);
  tft.setTextColor(sw.finished ? C(255, 60, 60) : th.accent, th.card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.drawString(buf, SCREEN_WIDTH / 2, 100);
  tft.setTextDatum(TL_DATUM);

  int btnY = 160;
  drawButton(10,  btnY, 100, 44, sw.running ? C(255,160,0) : C(0,220,100),
             sw.running ? "Pause" : "Start", 0x0000);
  drawButton(130, btnY, 100, 44, C(255,60,60), "Reset", 0xFFFFFF);

  drawButton(10, btnY + 56, SCREEN_WIDTH - 20, 38, th.card,
             sw.mode == SW_STOPWATCH ? "Mode: Stopwatch" : "Mode: Timer", th.text);

  if (sw.mode == SW_TIMER) {
    int setY = btnY + 110;
    tft.setTextColor(th.text, th.bg);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(F("Set time (H:M:S):"), SCREEN_WIDTH / 2, setY - 8);

    const int fieldW = 56, fieldH = 40;
    int fx = 18;
    for (int i = 0; i < 3; i++) {
      uint16_t c = (i == sw.selectedField) ? th.accent : th.card;
      drawSmoothCard(fx + i * (fieldW + 8), setY, fieldW, fieldH, c);
      tft.setTextColor((i == sw.selectedField) ? 0x0000 : th.text, c);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      int v = (i == 0) ? sw.setHours : (i == 1) ? sw.setMinutes : sw.setSeconds;
      char vb[4]; sprintf(vb, "%02d", v);
      tft.drawString(vb, fx + i * (fieldW + 8) + fieldW / 2, setY + fieldH / 2);
      tft.setTextDatum(TL_DATUM);
    }

    int adjY = setY + fieldH + 4;
    int adjH = 30;
    for (int i = 0; i < 3; i++) {
      int bx = fx + i * (fieldW + 8);
      drawButton(bx, adjY, fieldW / 2 - 2, adjH, th.card, "-", th.text);
      drawButton(bx + fieldW / 2 + 2, adjY, fieldW / 2 - 2, adjH, th.card, "+", th.text);
    }
  }
}

void stopwatchLoop() {
  if (!sw.running) return;
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 50) return;
  lastDraw = millis();

  unsigned long displayMs;
  if (sw.mode == SW_STOPWATCH) {
    displayMs = millis() - sw.startMs + sw.elapsedMs;
  } else {
    unsigned long elapsed = millis() - sw.startMs + sw.elapsedMs;
    if (elapsed >= sw.targetMs) {
      displayMs = 0;
      if (!sw.finished) {
        sw.finished = true;
        sw.running = false;
        sw.elapsedMs = sw.targetMs;
      }
    } else {
      displayMs = sw.targetMs - elapsed;
    }
  }

  Theme& th = themes[currentTheme];
  drawSmoothCard(10, 60, SCREEN_WIDTH - 20, 80, th.card);
  char buf[24];
  formatTime(displayMs, buf);
  tft.setTextColor(sw.finished ? C(255, 60, 60) : th.accent, th.card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);
  tft.drawString(buf, SCREEN_WIDTH / 2, 100);
  tft.setTextDatum(TL_DATUM);
}

void handleStopwatchTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }

  int btnY = 160;
  if (x > 10 && x < 110 && y > btnY && y < btnY + 44) {
    if (sw.running) {
      sw.elapsedMs += millis() - sw.startMs;
      sw.running = false;
    } else {
      if (sw.finished) { sw.finished = false; sw.elapsedMs = 0; }
      sw.startMs = millis();
      sw.running = true;
    }
    drawStopwatch();
    return;
  }
  if (x > 130 && x < 230 && y > btnY && y < btnY + 44) {
    sw.running = false;
    sw.elapsedMs = 0;
    sw.finished = false;
    drawStopwatch();
    return;
  }
  if (x > 10 && x < SCREEN_WIDTH - 10 && y > btnY + 56 && y < btnY + 94) {
    sw.mode = (sw.mode == SW_STOPWATCH) ? SW_TIMER : SW_STOPWATCH;
    sw.running = false;
    sw.elapsedMs = 0;
    sw.finished = false;
    if (sw.mode == SW_TIMER) {
      sw.targetMs = (sw.setHours * 3600UL + sw.setMinutes * 60UL + sw.setSeconds) * 1000UL;
    }
    drawStopwatch();
    return;
  }

  if (sw.mode == SW_TIMER) {
    int setY = btnY + 110;
    const int fieldW = 56, fieldH = 40;
    int fx = 18;
    for (int i = 0; i < 3; i++) {
      int bx = fx + i * (fieldW + 8);
      if (x > bx && x < bx + fieldW && y > setY && y < setY + fieldH) {
        sw.selectedField = i;
        drawStopwatch();
        return;
      }
    }
    int adjY = setY + fieldH + 4;
    int adjH = 30;
    for (int i = 0; i < 3; i++) {
      int bx = fx + i * (fieldW + 8);
      if (y > adjY && y < adjY + adjH) {
        int delta = (x < bx + fieldW / 2) ? -1 : +1;
        if      (i == 0) sw.setHours   = (sw.setHours   + delta + 24) % 24;
        else if (i == 1) sw.setMinutes = (sw.setMinutes + delta + 60) % 60;
        else             sw.setSeconds = (sw.setSeconds + delta + 60) % 60;
        sw.targetMs = (sw.setHours * 3600UL + sw.setMinutes * 60UL + sw.setSeconds) * 1000UL;
        sw.elapsedMs = 0;
        sw.finished = false;
        drawStopwatch();
        return;
      }
    }
  }
}

// ============================================================
//  PC MACRO
// ============================================================
void macroTapKey(uint8_t key) {
  Keyboard.write(key);
  delay(MACRO_DELAY_SHORT);
}

void macroTapKeyWithMod(uint8_t mod, uint8_t key) {
  Keyboard.press(mod);
  delay(20);
  Keyboard.write(key);
  Keyboard.releaseAll();
  delay(MACRO_DELAY_SHORT);
}

void drawPcMacro() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("PC Macro", true);

  bool connected = bleDevice.isConnected();
  drawSmoothCard(10, 54, SCREEN_WIDTH - 20, 40, th.card);
  tft.setTextColor(th.subtext, th.card);
  tft.setTextSize(1);
  tft.setCursor(20, 62);
  tft.print(F("Bluetooth status:"));
  tft.setTextColor(connected ? C(0,220,100) : C(255,120,0), th.card);
  tft.setTextSize(2);
  tft.setCursor(20, 76);
  tft.print(connected ? F("Connected") : F("Waiting..."));

  int y = 108;
  int btnH = 48;
  int gap  = 8;

  drawButton(10, y, SCREEN_WIDTH - 20, btnH, C(255,160,0), "Win+R cmd", 0x0000); y += btnH + gap;
  drawButton(10, y, SCREEN_WIDTH - 20, btnH, C(120,120,220), "Lock PC", 0xFFFFFF);  y += btnH + gap;
  drawButton(10, y, SCREEN_WIDTH - 20, btnH, C(255,60,60),  "Close Window", 0xFFFFFF);

  tft.setTextColor(th.subtext, th.bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString(F("Pair with 'ESP32 BLE Combo' on PC"), SCREEN_WIDTH / 2, SCREEN_HEIGHT - 20);
  tft.setTextDatum(TL_DATUM);
}

void runMacroCmd() {
  if (!bleDevice.isConnected()) return;
  Serial.println(F("[MACRO] Win+R cmd"));

  macroTapKeyWithMod(KEY_LEFT_GUI, 'r');
  delay(MACRO_DELAY_MED);

  macroTapKeyWithMod(KEY_LEFT_CTRL, 'a');
  delay(MACRO_DELAY_SHORT);

  macroTapKey(KEY_DELETE);
  delay(MACRO_DELAY_SHORT);

  Keyboard.print(F("cmd"));
  delay(MACRO_DELAY_SHORT);

  macroTapKey(KEY_RETURN);
  delay(MACRO_DELAY_LONG);

  macroTapKeyWithMod(KEY_LEFT_ALT, ' ');
  delay(MACRO_DELAY_MED);
  macroTapKey('x');
  delay(MACRO_DELAY_MED);

  Keyboard.print(F("color a"));
  delay(MACRO_DELAY_SHORT);
  macroTapKey(KEY_RETURN);
  delay(MACRO_DELAY_MED);

  Keyboard.print(F("dir /s"));
  delay(MACRO_DELAY_SHORT);
  macroTapKey(KEY_RETURN);

  Serial.println(F("[MACRO] Done"));
}

void runMacroLock() {
  if (!bleDevice.isConnected()) return;
  Serial.println(F("[MACRO] Lock PC"));
  macroTapKeyWithMod(KEY_LEFT_GUI, 'l');
}

void runMacroClose() {
  if (!bleDevice.isConnected()) return;
  Serial.println(F("[MACRO] Close Window"));
  macroTapKeyWithMod(KEY_LEFT_ALT, KEY_F4);
}

void handlePcMacroTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }

  int btnY = 108;
  int btnH = 48;
  int gap  = 8;

  if (y > btnY && y < btnY + btnH) {
    if (bleDevice.isConnected()) { runMacroCmd(); drawPcMacro(); }
    return;
  }
  btnY += btnH + gap;
  if (y > btnY && y < btnY + btnH) {
    if (bleDevice.isConnected()) { runMacroLock(); drawPcMacro(); }
    return;
  }
  btnY += btnH + gap;
  if (y > btnY && y < btnY + btnH) {
    if (bleDevice.isConnected()) { runMacroClose(); drawPcMacro(); }
    return;
  }
}

// ============================================================
//  ШПОРЫ
// ============================================================
void drawCheatPage() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  char title[32];
  snprintf(title, sizeof(title), "Cheats %d/%d", pageIndex + 1, MAX_CHEAT_PAGES);
  drawHeader(title, true);

  prefs.begin("cheats", true);
  char key[16];
  snprintf(key, sizeof(key), "page%d", pageIndex);
  String text = prefs.getString(key, "");
  prefs.end();

  if (text.length() == 0) {
    drawSmoothCard(20, 120, SCREEN_WIDTH - 40, 80, th.card);
    tft.setTextColor(th.subtext, th.card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString(F("Empty"), SCREEN_WIDTH / 2, 150);
    tft.setTextSize(1);
    tft.drawString(F("Add via web"), SCREEN_WIDTH / 2, 175);
    tft.setTextDatum(TL_DATUM);
  } else {
    const int MAX_LINES = 12;
    String lines[MAX_LINES];
    int lineCount = 0;
    int start = 0;
    for (int i = 0; i <= (int)text.length() && lineCount < MAX_LINES; i++) {
      if (i == (int)text.length() || text[i] == '\n') {
        lines[lineCount++] = text.substring(start, i);
        start = i + 1;
      }
    }
    int y = HEADER_HEIGHT + 10;
    for (int i = 0; i < lineCount; i++) {
      if (lines[i].length() == 0) { y += 12; continue; }
      String remaining = lines[i];
      while (remaining.length() > 0 && y < SCREEN_HEIGHT - 50) {
        String chunk = remaining.substring(0, min((int)remaining.length(), 28));
        remaining = remaining.substring(chunk.length());
        drawSmoothCard(8, y, SCREEN_WIDTH - 16, 26, th.card);
        tft.setTextColor(th.text, th.card);
        tft.setTextSize(1);
        tft.setCursor(20, y + 9);
        tft.print(chunk);
        y += 30;
      }
    }
  }

  int btnY = SCREEN_HEIGHT - 40;
  drawButton(8,   btnY, 108, 32, th.header, "< Back", th.text);
  drawButton(124, btnY, 108, 32, th.accent, "Next >", 0x0000);
}

void handlePageTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }
  int btnY = SCREEN_HEIGHT - 40;
  if (y > btnY && y < btnY + 32) {
    if (x > 8   && x < 116) { if (pageIndex > 0) pageIndex--; drawCheatPage(); return; }
    if (x > 124 && x < 232) { if (pageIndex < MAX_CHEAT_PAGES - 1) pageIndex++; drawCheatPage(); return; }
  }
}

// ============================================================
//  НАСТРОЙКИ
// ============================================================
void drawSettings() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Settings", true);

  drawSmoothCard(12, 54, SCREEN_WIDTH - 24, 48, th.card);
  tft.setTextColor(th.subtext, th.card);
  tft.setTextSize(1);
  tft.setCursor(22, 60);  tft.print(F("Theme:"));
  tft.setTextColor(th.accent, th.card);
  tft.setTextSize(2);
  tft.setCursor(22, 76);  tft.print(themes[currentTheme].name);

  drawSmoothCard(12, 108, SCREEN_WIDTH - 24, 48, th.card);
  tft.setTextColor(th.subtext, th.card);
  tft.setTextSize(1);
  tft.setCursor(22, 114); tft.print(F("Menu background:"));
  tft.setTextColor(th.accent, th.card);
  tft.setTextSize(2);
  tft.setCursor(22, 130); tft.print(bgColorNames[currentBgIndex]);

  drawSmoothCard(12, 162, SCREEN_WIDTH - 24, 70, th.card);
  tft.setTextColor(th.subtext, th.card);
  tft.setTextSize(1);
  tft.setCursor(22, 170); tft.print(F("WiFi: ")); tft.print(ap_ssid);
  tft.setCursor(22, 188); tft.print(F("IP: "));   tft.print(WiFi.softAPIP());
  tft.setCursor(22, 206); tft.print(F("Open in browser"));

  drawButton(12,  244, 104, 32, th.accent,  "Theme",     0x0000);
  drawButton(124, 244, 104, 32, th.accent2, "Wallpaper", 0x0000);
}

void handleSettingsTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }
  if (x > 12  && x < 116 && y > 244 && y < 276) {
    currentMode = MODE_THEME_SELECT; drawThemeSelect(); return;
  }
  if (x > 124 && x < 228 && y > 244 && y < 276) {
    currentMode = MODE_BG_COLOR_SELECT; drawBgColorSelect(); return;
  }
}

void drawThemeSelect() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Theme", true);

  int y = HEADER_HEIGHT + 8;
  for (int i = 0; i < THEME_COUNT; i++) {
    tft.fillRoundRect(12, y, SCREEN_WIDTH - 24, 36, 10, themes[i].card);
    tft.drawRoundRect(12, y, SCREEN_WIDTH - 24, 36, 10,
                      (i == currentTheme) ? th.accent : themes[i].subtext);
    tft.setTextColor(themes[i].text, themes[i].card);
    tft.setTextSize(2);
    tft.setCursor(24, y + 10);
    tft.print(themes[i].name);
    tft.fillCircle(150, y + 18, 7, themes[i].accent);
    tft.fillCircle(172, y + 18, 7, themes[i].accent2);
    tft.fillCircle(194, y + 18, 7, themes[i].header);
    y += 42;
  }
}

void handleThemeTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_SETTINGS; drawSettings(); return; }
  int itemY = HEADER_HEIGHT + 8;
  for (int i = 0; i < THEME_COUNT; i++) {
    if (x > 12 && x < SCREEN_WIDTH - 12 && y > itemY && y < itemY + 36) {
      currentTheme = i;
      prefs.begin("settings", false);
      prefs.putInt("theme", currentTheme);
      prefs.end();
      drawThemeSelect();
      return;
    }
    itemY += 42;
  }
}

void drawBgColorSelect() {
  Theme& th = themes[currentTheme];
  tft.fillScreen(th.bg);
  drawHeader("Wallpaper", true);

  int y = HEADER_HEIGHT + 6;
  for (int i = 0; i < 8; i++) {
    uint16_t c = (i == 0) ? th.bg : bgColors[i];
    tft.fillRoundRect(12, y, SCREEN_WIDTH - 24, 30, 8, c);
    tft.drawRoundRect(12, y, SCREEN_WIDTH - 24, 30, 8,
                      (i == currentBgIndex) ? th.accent : th.subtext);
    tft.setTextColor(th.text, c);
    tft.setTextSize(1);
    tft.setCursor(24, y + 10);
    tft.print(bgColorNames[i]);
    y += 34;
  }
}

void handleBgColorTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_SETTINGS; drawSettings(); return; }
  int itemY = HEADER_HEIGHT + 6;
  for (int i = 0; i < 8; i++) {
    if (x > 12 && x < SCREEN_WIDTH - 12 && y > itemY && y < itemY + 30) {
      currentBgIndex = i;
      prefs.begin("settings", false);
      prefs.putInt("bg", currentBgIndex);
      prefs.end();
      drawBgColorSelect();
      return;
    }
    itemY += 34;
  }
}

// ============================================================
//  MATRIX
// ============================================================
void matrixInit() {
  for (int i = 0; i < MTX_COLS; i++) {
    mtxHead[i] = random(-MTX_ROW_COUNT, 0);
    mtxLen[i]  = random(4, MTX_ROW_COUNT / 2);
  }
  tft.fillScreen(0x0000);
  drawHeader("Matrix", true);
  tft.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT, 0x0000);
  mtxLastTick  = millis();
  mtxLastFrame = millis();
}

void matrixTick() {
  if (millis() - mtxLastTick < 60) return;
  mtxLastTick = millis();
  for (int i = 0; i < MTX_COLS; i++) {
    mtxHead[i]++;
    if (mtxHead[i] - mtxLen[i] > MTX_ROW_COUNT) {
      tft.fillRect(i * MTX_CHAR_W + 2, HEADER_HEIGHT,
                   4, SCREEN_HEIGHT - HEADER_HEIGHT, 0x0000);
      mtxHead[i] = random(-MTX_ROW_COUNT / 2, 0);
      mtxLen[i]  = random(4, MTX_ROW_COUNT / 2);
    }
  }
}

void matrixDraw() {
  for (int i = 0; i < MTX_COLS; i++) {
    int x = i * MTX_CHAR_W;
    int headRow = mtxHead[i];
    int tailRow = mtxHead[i] - mtxLen[i] - 1;

    int clearY = tailRow * MTX_CHAR_H;
    if (clearY >= HEADER_HEIGHT && clearY < SCREEN_HEIGHT) {
      tft.fillRect(x + 2, clearY, 4, MTX_CHAR_H - 2, 0x0000);
    }

    int headY = headRow * MTX_CHAR_H;
    if (headY >= HEADER_HEIGHT && headY < SCREEN_HEIGHT) {
      tft.fillRect(x + 2, headY, 4, MTX_CHAR_H - 2, C(220, 255, 220));
    }

    for (int j = 1; j <= mtxLen[i]; j++) {
      int y = (headRow - j) * MTX_CHAR_H;
      if (y < HEADER_HEIGHT || y >= SCREEN_HEIGHT) continue;
      long gLong = map(j, 1, mtxLen[i] + 1, 255, 30);
      uint8_t g = (uint8_t)constrain(gLong, 0, 255);
      tft.fillRect(x + 2, y, 4, MTX_CHAR_H - 2, C(0, g, 0));
    }
  }
}

void handleMatrixTouch(int x, int y) {
  if (x < 60 && y < 50) { currentMode = MODE_MENU; drawMenu(); return; }
}

// ============================================================
//  ВЕБ-СЕРВЕР
// ============================================================
void setupWebServer() {
  server.on("/",            handleRoot);
  server.on("/api/pages",   handleAPIPages);
  server.on("/api/save",    HTTP_POST, handleSave);
  server.on("/api/clear",   HTTP_POST, handleClear);
  server.begin();
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Linux</title>
  <style>
    *{margin:0;padding:0;box-sizing:border-box}
    body{font-family:-apple-system,system-ui,sans-serif;background:#0a0a0a;color:#fff;padding:16px;max-width:460px;margin:0 auto}
    h1{text-align:center;background:linear-gradient(135deg,#00e678,#00c8ff);-webkit-background-clip:text;-webkit-text-fill-color:transparent;font-size:1.4em;margin:10px 0;font-weight:800}
    h2{font-size:1em;color:#00e678;margin:18px 0 8px}
    .page{background:#1a1a1a;padding:10px;margin:8px 0;border-radius:14px;border:1px solid #2a2a2a}
    .page b{display:block;margin-bottom:6px;color:#00e678;font-size:0.85em}
    textarea{width:100%;height:70px;background:#0a0a0a;color:#fff;border:1px solid #333;border-radius:8px;padding:8px;font-size:0.85em;resize:vertical;font-family:inherit}
    textarea:focus{outline:none;border-color:#00e678}
    .btn-row{display:flex;gap:8px;margin-top:10px}
    button{padding:12px 16px;border:none;border-radius:20px;cursor:pointer;font-weight:700;font-size:0.85em}
    button:active{transform:scale(0.97)}
    .save{background:linear-gradient(135deg,#00e678,#00a050);color:#fff;flex:1}
    .clear{background:#2a2a2a;color:#ff5050;flex:1}
    #status{text-align:center;margin:10px 0;min-height:20px;font-size:0.85em;color:#00e678}
    .count{text-align:center;color:#666;font-size:0.75em;margin-top:8px}
  </style>
</head>
<body>
  <h1>Linux</h1>
  <h2>Cheat sheets</h2>
  <div id="pages"></div>
  <div class="btn-row">
    <button class="save" onclick="saveAll()">Save</button>
    <button class="clear" onclick="clearAll()">Clear</button>
  </div>
  <p class="count">Pages: <span id="cnt">0</span> / 20</p>
  <p id="status"></p>
  <script>
    const MAX=20;
    function loadPages(){
      fetch('/api/pages').then(r=>r.json()).then(data=>{
        let cnt=0;
        for(let i=0;i<MAX;i++){
          const d=document.createElement('div');
          d.className='page';
          d.innerHTML='<b>Page '+(i+1)+'</b><textarea id="p'+i+'">'+(data[i]||'')+'</textarea>';
          document.getElementById('pages').appendChild(d);
          if((data[i]||'').trim())cnt++;
        }
        document.getElementById('cnt').textContent=cnt;
      });
    }
    function saveAll(){
      let data={},cnt=0;
      for(let i=0;i<MAX;i++){
        const v=document.getElementById('p'+i).value;
        if(v.trim()){data[i]=v;cnt++;}
      }
      fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
      .then(r=>r.text()).then(msg=>{
        document.getElementById('status').textContent='OK '+msg;
        document.getElementById('cnt').textContent=cnt;
      });
    }
    function clearAll(){
      if(confirm('Clear all cheat sheets?')){
        fetch('/api/clear',{method:'POST'}).then(r=>r.text()).then(msg=>{
          for(let i=0;i<MAX;i++)document.getElementById('p'+i).value='';
          document.getElementById('status').textContent='OK '+msg;
          document.getElementById('cnt').textContent='0';
        });
      }
    }
    loadPages();
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

static String jsonEscape(const String& in) {
  String out; out.reserve(in.length() + 8);
  for (size_t j = 0; j < in.length(); j++) {
    char c = in[j];
    if      (c == '"')  out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else                out += c;
  }
  return out;
}

void handleAPIPages() {
  prefs.begin("cheats", true);
  String json = "{";
  for (int i = 0; i < MAX_CHEAT_PAGES; i++) {
    if (i > 0) json += ",";
    char key[16];
    snprintf(key, sizeof(key), "page%d", i);
    String val = prefs.getString(key, "");
    json += "\"" + String(i) + "\":\"" + jsonEscape(val) + "\"";
  }
  prefs.end();
  json += "}";
  server.send(200, "application/json", json);
}

void handleSave() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "No data"); return; }
  String body = server.arg("plain");

  String parsed[MAX_CHEAT_PAGES];
  bool   filled[MAX_CHEAT_PAGES];
  for (int i = 0; i < MAX_CHEAT_PAGES; i++) { parsed[i] = ""; filled[i] = false; }

  int pos = 0, len = body.length();
  int parsedCount = 0;
  bool parseError = false;

  while (pos < len) {
    while (pos < len && (body[pos] == ',' || body[pos] == ' ' || body[pos] == '\n' ||
                         body[pos] == '\r' || body[pos] == '\t')) pos++;
    if (pos >= len) break;
    if (body[pos] != '"') { parseError = true; break; }

    int keyStart = pos + 1;
    int keyEnd = body.indexOf('"', keyStart); if (keyEnd < 0) { parseError = true; break; }
    String key = body.substring(keyStart, keyEnd);
    if (key.length() == 0 || !isDigit(key[0])) { parseError = true; break; }

    int colon = body.indexOf(':', keyEnd); if (colon < 0) { parseError = true; break; }
    int valStart = body.indexOf('"', colon); if (valStart < 0) { parseError = true; break; }

    String value = "";
    int i = valStart + 1;
    bool closed = false;
    while (i < len) {
      char c = body[i];
      if (c == '\\' && i + 1 < len) {
        char n = body[i + 1];
        if      (n == 'n')  value += '\n';
        else if (n == 'r')  value += '\r';
        else if (n == '"')  value += '"';
        else if (n == '\\') value += '\\';
        else                value += n;
        i += 2;
      } else if (c == '"') { closed = true; i++; break; }
      else { value += c; i++; }
    }
    if (!closed) { parseError = true; break; }
    pos = i;

    int pageNum = key.toInt();
    if (pageNum < 0 || pageNum >= MAX_CHEAT_PAGES) { parseError = true; break; }

    parsed[pageNum] = value;
    filled[pageNum] = true;
    parsedCount++;
  }

  if (parseError) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }

  prefs.begin("cheats", false);
  for (int i = 0; i < MAX_CHEAT_PAGES; i++) {
    char k[16]; snprintf(k, sizeof(k), "page%d", i);
    prefs.remove(k);
  }
  for (int i = 0; i < MAX_CHEAT_PAGES; i++) {
    if (filled[i] && parsed[i].length() > 0) {
      char k[16]; snprintf(k, sizeof(k), "page%d", i);
      prefs.putString(k, parsed[i]);
    }
  }
  prefs.end();

  server.send(200, "text/plain", "Saved " + String(parsedCount));
}

void handleClear() {
  prefs.begin("cheats", false);
  prefs.clear();
  prefs.end();
  server.send(200, "text/plain", "Cleared");
}
