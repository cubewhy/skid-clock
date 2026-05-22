#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <math.h>

// ==========================================
// 1. 系统与硬件引脚配置
// ==========================================
#define OLED_SDA 32
#define OLED_SCL 33
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define RTC_CLK 4
#define RTC_DAT 16
#define RTC_RST 17

#define JOY_VRX 12  // 左右控制 (X轴)
#define JOY_VRY 14  // 上下控制 (Y轴)
#define JOY_SW 27   // 摇杆按键
#define BTN_BACK 15 // 返回/退出物理按键
#define POT_PIN 34  // 打砖块旋钮电位器引脚

#define GITHUB_URL "github.com/cubewhy"
#define IDLE_TIMEOUT 180000 // 闲置防烧屏/触发宠物的时间阈值 (3分钟 = 180000ms)

// ==========================================
// 2. 系统状态机与菜单全局定义
// ==========================================
enum SystemState {
  STATE_MAIN_MENU,   // 主菜单选择界面
  STATE_CLOCK,       // 正常时钟运行
  STATE_CALCULATOR,  // 表达式计算器模式
  STATE_GAMES_MENU,  // 游戏二级菜单
  STATE_SETTINGS,    // 调时设置模式
  STATE_SNAKE,       // 贪吃蛇游戏模式
  STATE_GOMOKU_MENU, // 五子棋配置菜单
  STATE_GOMOKU_PLAY, // 五子棋游戏对局
  STATE_2048,        // 2048游戏模式
  STATE_DINO,        // 谷歌小恐龙模式
  STATE_BRICK,       // 打砖块游戏模式
  STATE_STACK,       // 叠罗汉/堆叠游戏模式
  STATE_PET,         // 桌面宠物模式
  STATE_TIMERS_MENU, // 时间工具二级菜单
  STATE_STOPWATCH,   // 正计时（秒表）
  STATE_COUNTDOWN,   // 倒计时
  STATE_POMODORO     // 番茄钟
};
SystemState currentState = STATE_CLOCK;

// 主菜单配置
const char *menuItems[] = {"1. Realtime Clock", "2. Time Tools",
                           "3. Arcade Games",   "4. Calculator",
                           "5. Desktop Pet",    "6. Adjust Settings"};
const int MENU_TOTAL = 6;
const int VISIBLE_ITEMS = 3; // 滚动菜单可见行数
int currentMenuSelect = 0;
int menuScrollTop = 0; // 滚动视口顶部索引

// 时间工具二级菜单配置
const char *timerMenuItems[] = {"1. Stopwatch", "2. Countdown", "3. Pomodoro",
                                "4. < Back"};
const int TIMERS_TOTAL = 4;
int currentTimersSelect = 0;

// 游戏二级菜单配置
const char *gameMenuItems[] = {
    "1. Snake Game",    "2. Gomoku Game", "3. 2048 Game", "4. Dino Run",
    "5. Brick Breaker", "6. Stack Tower", "7. < Back"};
const int GAMES_TOTAL = 7;
const int VISIBLE_GAMES_ITEMS = 3; // 游戏菜单可见行数
int currentGamesSelect = 0;
int gamesScrollTop = 0; // 游戏窗口滚动顶部索引

// ==========================================
// 3. 各模块核心变量
// ==========================================

// --- 闲置状态检测 ---
unsigned long lastActivityTime = 0;

// --- 正计时（秒表）变量 ---
unsigned long stopwatchElapsed = 0; // 累计毫秒数
unsigned long stopwatchLastTime = 0;
bool stopwatchRunning = false;

// --- 倒计时变量 ---
long countdownTotalSec = 300; // 默认5分钟 (300秒)
unsigned long countdownStartTime = 0;
long countdownRemainingSec = 300;
bool countdownRunning = false;
bool countdownSettingMode = true; // 或者是运行模式
int countdownEditField = 0;       // 0:分, 1:秒

// --- 番茄钟变量 ---
enum PomoState { POMO_WORK, POMO_BREAK, POMO_PAUSE };
PomoState pomoState = POMO_PAUSE;
PomoState pomoPrevState = POMO_WORK; // 记录暂停前的状态（工作/短休）
long pomoRemainingSec = 1500;        // 25分钟 = 1500秒
unsigned long pomoLastTick = 0;
int pomoCompletedCycles = 0;
int pomoConfigWorkMin = 25; // 可配置工作时长

// --- 桌面宠物(猫咪)模块 ---
enum PetState { PET_IDLE, PET_WALK, PET_SLEEP };
PetState petState = PET_IDLE;
int petX = 54;
int petY = 40;
int petDir = 1; // 1: 右, -1: 左
unsigned long lastPetStateChange = 0;
unsigned long lastPetAnimUpdate = 0;
int petFrame = 0;
bool petEnteredViaTimeout = false; // 标记是否是通过超时挂机进入的宠物模式

// --- 时钟设置模块 ---
enum EditField {
  FIELD_YEAR,
  FIELD_MONTH,
  FIELD_DAY,
  FIELD_HOUR,
  FIELD_MINUTE,
  FIELD_SECOND
};
EditField currentEditField = FIELD_YEAR;

int editYear = 2026, editMonth = 1, editDay = 1;
int editHour = 0, editMinute = 0, editSecond = 0;
unsigned long lastSettingsTick = 0;

int getMaxDay(int y, int m) {
  if (m == 2) {
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
      return 29;
    return 28;
  }
  if (m == 4 || m == 6 || m == 9 || m == 11)
    return 30;
  return 31;
}

// --- 计算器模块 ---
int calcCx = 0, calcCy = 0;   // 光标坐标
char calcInput[32] = "";      // 输入缓冲区
char calcResultStr[24] = "";  // 结果显示区
bool calcClearOnNext = false; // 是否在下次按键时清空输入

const char calcKeys[4][6] = {
    {'7', '8', '9', '/', '(', 'C'}, // C: Clear
    {'4', '5', '6', '*', ')', 'D'}, // D: Delete (Backspace)
    {'1', '2', '3', '-', '^', 'S'}, // S: Sqrt
    {'0', '.', '=', '+', 'P', ' '}  // P: Pi, 留空一位
};

// --- 贪吃蛇游戏模块 ---
#define SNAKE_BLOCK_SIZE 4
#define GRID_WIDTH (SCREEN_WIDTH / SNAKE_BLOCK_SIZE)   // 32
#define GRID_HEIGHT (SCREEN_HEIGHT / SNAKE_BLOCK_SIZE) // 16
#define SNAKE_MAX_LEN 64

enum SnakeDirection { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT };
struct Point {
  int8_t x;
  int8_t y;
};

Point snake[SNAKE_MAX_LEN];
int snakeLength = 3;
SnakeDirection snakeDir = SNAKE_RIGHT;
Point food;
bool isGameOver = false;
unsigned long lastSnakeUpdate = 0;
const int snakeSpeed = 150;

// --- 五子棋游戏模块 ---
#define GOMOKU_SIZE 10
int8_t gomokuBoard[GOMOKU_SIZE][GOMOKU_SIZE];
int8_t gomokuCx = 4, gomokuCy = 4;
bool gomokuIsPvE = true;
int gomokuDiff = 1;
int8_t gomokuTurn = 1;
int8_t gomokuWinner = 0;
int gomokuMenuSelect = 0;

// --- 2048游戏模块 ---
int16_t board2048[4][4];
int score2048 = 0;
bool gameOver2048 = false;

// --- 谷歌小恐龙模块 ---
float dinoY = 40.0f;
float dinoVelocityY = 0.0f;
bool dinoIsJumping = false;
bool dinoIsDucking = false;
float scoreDino = 0.0f;
bool gameOverDino = false;
unsigned long lastDinoUpdate = 0;

float dinoGameSpeed = 0.10f;
const float DINO_GRAVITY = 0.00055f;
const float DINO_JUMP_FORCE = -0.17f;

struct Obstacle {
  float x;
  int16_t y;
  uint8_t type;
  bool active;
};
#define MAX_OBSTACLES 2
Obstacle obstacles[MAX_OBSTACLES];
unsigned long lastObstacleSpawn = 0;

// --- 打砖块游戏模块 ---
float brickBallX, brickBallY;
float brickBallVX, brickBallVY;
int brickPaddleX;
const int brickPaddleWidth = 24;
const int brickPaddleHeight = 3;
const int brickPaddleY = 58;

#define BRICK_ROWS 3
#define BRICK_COLS 6
bool brickGrid[BRICK_ROWS][BRICK_COLS];
const int brickWidth = 18;
const int brickHeight = 4;
const int brickGapX = 3;
const int brickGapY = 3;
const int brickOffsetX = 2;
const int brickOffsetY = 13;

int scoreBrick = 0;
bool gameOverBrick = false;
bool gameWinBrick = false;
unsigned long lastBrickUpdate = 0;

// --- Stack 叠罗汉游戏全局变量 ---
#define MAX_STACK_LAYERS 40
#define STACK_BLOCK_HEIGHT 5

float stackBlockX = 0.0f;
int stackBlockWidth = 45;
float stackBlockSpeedX = 1.4f;
bool stackIsFalling = false;
float stackBlockY = 12.0f;

int towerLayerCount = 0;
int scoreStack = 0;
bool gameOverStack = false;
unsigned long lastStackTick = 0;

int towerWidths[MAX_STACK_LAYERS];
int towerXs[MAX_STACK_LAYERS];
int cameraViewOffsetY = 0;
bool joyMoveLatched = false;

// --- 输入与节流控制 ---
unsigned long lastButtonPress = 0;
unsigned long lastJoyAction = 0;
const int DEBOUNCE_DELAY = 250;
const int JOY_DELAY = 200;

unsigned long backBtnDownTime = 0;
int lastBackState = HIGH;
bool longPressTriggered = false;

// ==========================================
// 4. 核心驱动对象初始化
// ==========================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

// ==========================================
// 5. 函数前置声明
// ==========================================
void handleMainMenu(int vry, int vrx, bool clicked);
void handleClockMode(bool clicked);
void handleCalculatorMode(int vry, int vrx, bool clicked);
void handleGamesMenu(int vry, int vrx, bool clicked);
void handleSettingsMode(int vry, int vrx, bool clicked);
void handleSnakeMode(int vry, int vrx, bool clicked);
void initSnakeGame();
void spawnFood();

void handleGomokuMenu(int vry, int vrx, bool clicked);
void handleGomokuPlay(int vry, int vrx, bool clicked);
void initGomokuGame();
bool checkGomokuWin(int x, int y, int role);
void gomokuAIMove();
int evaluateGomokuPoint(int x, int y, int role);

void handle2048Mode(int vry, int vrx, bool clicked);
void init2048Game();
void spawn2048Tile();
bool check2048GameOver();

void handleDinoMode(int vry, int vrx, bool clicked);
void initDinoGame();

void handleBrickMode(bool clicked);
void initBrickGame();

void handleStackMode(int vry, int vrx, bool clicked);
void initStackGame();

void handlePetMode(int vry, int vrx, bool clicked);
void initPet();

void handleTimersMenu(int vry, int vrx, bool clicked);
void handleStopwatch(int vry, int vrx, bool clicked);
void handleCountdown(int vry, int vrx, bool clicked);
void handlePomodoro(int vry, int vrx, bool clicked);

double parseExpression(const char *&p);
double parseTerm(const char *&p);
double parsePower(const char *&p);
double parseAtom(const char *&p);

// ==========================================
// 6. 主程序入口 (Setup & Loop)
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(JOY_VRX, INPUT);
  pinMode(JOY_VRY, INPUT);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  analogSetAttenuation(ADC_11db);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  Rtc.Begin();
  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
    Rtc.SetDateTime(compiled);
  }

  randomSeed(analogRead(POT_PIN));
  lastActivityTime = millis();
}

void loop() {
  int swState = digitalRead(JOY_SW);
  int vrxVal = analogRead(JOY_VRX);
  int vryVal = analogRead(JOY_VRY);

  bool anyInputDetected = false;

  bool isClicked = false;
  if (swState == LOW && (millis() - lastButtonPress > DEBOUNCE_DELAY)) {
    lastButtonPress = millis();
    isClicked = true;
    anyInputDetected = true;
  }

  if (vrxVal < 1000 || vrxVal > 3000 || vryVal < 1000 || vryVal > 3000) {
    anyInputDetected = true;
  }

  int currentBackState = digitalRead(BTN_BACK);
  bool shortPress = false;
  bool longPress = false;

  if (currentBackState != lastBackState) {
    if (currentBackState == HIGH) {
      if (!longPressTriggered && (millis() - backBtnDownTime >= 50)) {
        shortPress = true;
        anyInputDetected = true;
      }
      longPressTriggered = false;
    } else {
      backBtnDownTime = millis();
    }
    lastBackState = currentBackState;
  } else if (currentBackState == LOW && !longPressTriggered) {
    if (millis() - backBtnDownTime > 800) {
      longPress = true;
      longPressTriggered = true;
      anyInputDetected = true;
    }
  }

  if (currentState == STATE_BRICK) {
    static int lastPotVal = 0;
    int currentPotVal = analogRead(POT_PIN);
    if (abs(currentPotVal - lastPotVal) > 40) {
      anyInputDetected = true;
    }
    lastPotVal = currentPotVal;
  }

  if (anyInputDetected) {
    lastActivityTime = millis();
  } else {
    if ((currentState == STATE_CLOCK || currentState == STATE_MAIN_MENU) &&
        (millis() - lastActivityTime > IDLE_TIMEOUT)) {
      petEnteredViaTimeout = true;
      initPet();
      currentState = STATE_PET;
      lastActivityTime = millis();
    }
  }

  // 返回键状态路由切换
  if (longPress) {
    currentState = STATE_MAIN_MENU;
  } else if (shortPress) {
    if (currentState == STATE_GOMOKU_PLAY) {
      currentState = STATE_GOMOKU_MENU;
    } else if (currentState == STATE_GOMOKU_MENU ||
               currentState == STATE_SNAKE || currentState == STATE_2048 ||
               currentState == STATE_DINO || currentState == STATE_BRICK ||
               currentState == STATE_STACK) {
      currentState = STATE_GAMES_MENU;
    } else if (currentState == STATE_PET) {
      if (petEnteredViaTimeout) {
        currentState = STATE_CLOCK;
      } else {
        currentState = STATE_MAIN_MENU;
      }
    } else if (currentState == STATE_GAMES_MENU ||
               currentState == STATE_CALCULATOR ||
               currentState == STATE_SETTINGS || currentState == STATE_CLOCK) {
      currentState = STATE_MAIN_MENU;
    } else if (currentState == STATE_MAIN_MENU) {
      currentState = STATE_CLOCK;
    } else if (currentState == STATE_TIMERS_MENU ||
               currentState == STATE_CALCULATOR ||
               currentState == STATE_SETTINGS || currentState == STATE_CLOCK) {
      currentState = STATE_MAIN_MENU;
    } else if (currentState == STATE_STOPWATCH ||
               currentState == STATE_COUNTDOWN ||
               currentState == STATE_POMODORO) {
      currentState = STATE_TIMERS_MENU;
    }
  }

  switch (currentState) {
  case STATE_MAIN_MENU:
    handleMainMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_CLOCK:
    if (isClicked ||
        (vrxVal < 1000 || vrxVal > 3000 || vryVal < 1000 || vryVal > 3000)) {
      if (isClicked || (millis() - lastJoyAction > JOY_DELAY)) {
        if (!isClicked) {
        }
        currentState = STATE_MAIN_MENU;
      }
    }
    handleClockMode(false);
    break;
  case STATE_CALCULATOR:
    handleCalculatorMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_GAMES_MENU:
    handleGamesMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_SETTINGS:
    handleSettingsMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_SNAKE:
    handleSnakeMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_GOMOKU_MENU:
    handleGomokuMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_GOMOKU_PLAY:
    handleGomokuPlay(vryVal, vrxVal, isClicked);
    break;
  case STATE_2048:
    handle2048Mode(vryVal, vrxVal, isClicked);
    break;
  case STATE_DINO:
    handleDinoMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_BRICK:
    handleBrickMode(isClicked);
    break;
  case STATE_STACK:
    handleStackMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_PET:
    handlePetMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_TIMERS_MENU:
    handleTimersMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_STOPWATCH:
    handleStopwatch(vryVal, vrxVal, isClicked);
    break;
  case STATE_COUNTDOWN:
    handleCountdown(vryVal, vrxVal, isClicked);
    break;
  case STATE_POMODORO:
    handlePomodoro(vryVal, vrxVal, isClicked);
    break;
  }

  display.display();
  delay(10);
}

// ==========================================
// 7. 模块化业务逻辑实现
// ==========================================

// --- 主菜单模块 (支持摇杆向左返回，向右选中) ---
void handleMainMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) { // 向上
      currentMenuSelect =
          (currentMenuSelect == 0) ? MENU_TOTAL - 1 : currentMenuSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) { // 向下
      currentMenuSelect = (currentMenuSelect + 1) % MENU_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) { // back
      currentState = STATE_CLOCK;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) { // confirm
      selectTriggered = true;
      lastJoyAction = millis();
    }

    if (currentMenuSelect < menuScrollTop) {
      menuScrollTop = currentMenuSelect;
    } else if (currentMenuSelect >= menuScrollTop + VISIBLE_ITEMS) {
      menuScrollTop = currentMenuSelect - VISIBLE_ITEMS + 1;
    }
  }

  if (selectTriggered) {
    switch (currentMenuSelect) {
    case 0:
      currentState = STATE_CLOCK;
      break;
    case 1:
      currentTimersSelect = 0;
      currentState = STATE_TIMERS_MENU;
      break;
    case 2:
      currentGamesSelect = 0;
      gamesScrollTop = 0;
      currentState = STATE_GAMES_MENU;
      break;
    case 3:
      calcInput[0] = '\0';
      calcResultStr[0] = '\0';
      calcClearOnNext = false;
      currentState = STATE_CALCULATOR;
      break;
    case 4:
      petEnteredViaTimeout = false;
      initPet();
      currentState = STATE_PET;
      break;
    case 5:
      RtcDateTime nowSettings = Rtc.GetDateTime();
      editYear = nowSettings.Year();
      editMonth = nowSettings.Month();
      editDay = nowSettings.Day();
      editHour = nowSettings.Hour();
      editMinute = nowSettings.Minute();
      editSecond = nowSettings.Second();
      currentEditField = FIELD_HOUR;
      lastSettingsTick = millis();
      currentState = STATE_SETTINGS;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== SYSTEM MENU ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < VISIBLE_ITEMS; i++) {
    int itemIdx = menuScrollTop + i;
    if (itemIdx >= MENU_TOTAL)
      break;

    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (itemIdx == currentMenuSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(menuItems[itemIdx]);
  }

  int barX = 124, barY = 14, barHeight = 34;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_ITEMS / MENU_TOTAL;
  int thumbY =
      barY + ((barHeight - thumbHeight) * currentMenuSelect / (MENU_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);

  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.print(GITHUB_URL);
}

void initPet() {
  petState = PET_IDLE;
  petX = 54;
  petY = 40;
  petFrame = 0;
  lastPetStateChange = millis();
  lastPetAnimUpdate = millis();
}

void handlePetMode(int vry, int vrx, bool clicked) {
  if (clicked) {
    if (petEnteredViaTimeout) {
      currentState = STATE_CLOCK;
    } else {
      currentState = STATE_MAIN_MENU;
    }
    return;
  }

  unsigned long now = millis();

  if (now - lastPetStateChange > 5000) {
    lastPetStateChange = now;
    int r = random(0, 3);
    if (r == 0) {
      petState = PET_IDLE;
    } else if (r == 1) {
      petState = PET_WALK;
      petDir = (random(0, 2) == 0) ? 1 : -1;
    } else {
      petState = PET_SLEEP;
    }
  }

  int animInterval = (petState == PET_WALK) ? 140 : 350;
  if (now - lastPetAnimUpdate > animInterval) {
    lastPetAnimUpdate = now;
    petFrame = (petFrame + 1) % 4;

    if (petState == PET_WALK) {
      petX += petDir * 2;
      if (petX < 5) {
        petX = 5;
        petDir = 1;
      }
      if (petX > 107) {
        petX = 107;
        petDir = -1;
      }
    }
  }

  display.clearDisplay();
  display.drawFastHLine(0, 56, 128, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(F("Cat Life~"));

  RtcDateTime rtcNow = Rtc.GetDateTime();
  char petTimeStr[9];
  sprintf(petTimeStr, "%02d:%02d:%02d", rtcNow.Hour(), rtcNow.Minute(),
          rtcNow.Second());
  display.setCursor(80, 2);
  display.print(petTimeStr);

  int x = petX;
  int y = petY;

  if (petState == PET_SLEEP) {
    display.fillRoundRect(x, y + 6, 16, 10, 4, SSD1306_WHITE);
    display.drawFastHLine(x + 11, y + 10, 3, SSD1306_BLACK);
    display.fillTriangle(x + 9, y + 6, x + 11, y + 2, x + 13, y + 6,
                         SSD1306_BLACK);
    display.drawTriangle(x + 9, y + 6, x + 11, y + 2, x + 13, y + 6,
                         SSD1306_WHITE);
    if (petFrame == 0) {
      display.setCursor(x + 19, y + 2);
      display.print(F("z"));
    } else if (petFrame == 1) {
      display.setCursor(x + 22, y - 1);
      display.print(F("zZ"));
    } else if (petFrame == 2) {
      display.setCursor(x + 24, y - 5);
      display.print(F("ZzZ"));
    }
  } else {
    display.fillRoundRect(x + 2, y + 4, 12, 9, 3, SSD1306_WHITE);
    int hx = (petDir == 1) ? x + 9 : x - 3;
    display.fillRoundRect(hx, y - 1, 9, 8, 2, SSD1306_WHITE);

    if (petDir == 1) {
      display.fillTriangle(hx + 1, y - 1, hx + 3, y - 4, hx + 5, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 1, y - 1, hx + 3, y - 4, hx + 5, y - 1,
                           SSD1306_WHITE);
      display.fillTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                           SSD1306_WHITE);
      display.drawPixel(hx + 6, y + 2, SSD1306_BLACK);
    } else {
      display.fillTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                           SSD1306_WHITE);
      display.fillTriangle(hx + 4, y - 1, hx + 6, y - 4, hx + 8, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 4, y - 1, hx + 6, y - 4, hx + 8, y - 1,
                           SSD1306_WHITE);
      display.drawPixel(hx + 2, y + 2, SSD1306_BLACK);
    }

    if (petState == PET_WALK) {
      if (petFrame % 2 == 0) {
        display.drawLine(x + 4, y + 13, x + 2, y + 16, SSD1306_WHITE);
        display.drawLine(x + 7, y + 13, x + 7, y + 16, SSD1306_WHITE);
        display.drawLine(x + 10, y + 13, x + 9, y + 16, SSD1306_WHITE);
        display.drawLine(x + 13, y + 13, x + 14, y + 16, SSD1306_WHITE);
      } else {
        display.drawLine(x + 4, y + 13, x + 5, y + 16, SSD1306_WHITE);
        display.drawLine(x + 7, y + 13, x + 6, y + 16, SSD1306_WHITE);
        display.drawLine(x + 10, y + 13, x + 11, y + 16, SSD1306_WHITE);
        display.drawLine(x + 13, y + 13, x + 12, y + 16, SSD1306_WHITE);
      }
    } else {
      display.drawFastVLine(x + 4, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 6, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 10, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 12, y + 13, 3, SSD1306_WHITE);
    }

    int tx = (petDir == 1) ? x + 2 : x + 14;
    int tailWiggle = (petFrame % 2 == 0) ? -2 : 1;
    display.drawLine(tx, y + 6, tx - (petDir * 4), y + 2 + tailWiggle,
                     SSD1306_WHITE);
  }
}

// --- 游戏二级子菜单 (支持摇杆向左返回，向右选中) ---
void handleGamesMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) { // 向上
      currentGamesSelect =
          (currentGamesSelect == 0) ? GAMES_TOTAL - 1 : currentGamesSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) { // 向下
      currentGamesSelect = (currentGamesSelect + 1) % GAMES_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) { // back
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) { // confirm
      selectTriggered = true;
      lastJoyAction = millis();
    }

    if (currentGamesSelect < gamesScrollTop) {
      gamesScrollTop = currentGamesSelect;
    } else if (currentGamesSelect >= gamesScrollTop + VISIBLE_GAMES_ITEMS) {
      gamesScrollTop = currentGamesSelect - VISIBLE_GAMES_ITEMS + 1;
    }
  }

  if (selectTriggered) {
    switch (currentGamesSelect) {
    case 0:
      initSnakeGame();
      currentState = STATE_SNAKE;
      break;
    case 1:
      gomokuMenuSelect = 0;
      currentState = STATE_GOMOKU_MENU;
      break;
    case 2:
      init2048Game();
      currentState = STATE_2048;
      break;
    case 3:
      initDinoGame();
      currentState = STATE_DINO;
      break;
    case 4:
      initBrickGame();
      currentState = STATE_BRICK;
      break;
    case 5:
      initStackGame();
      currentState = STATE_STACK;
      break;
    case 6:
      currentState = STATE_MAIN_MENU;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== GAME CENTER ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < VISIBLE_GAMES_ITEMS; i++) {
    int itemIdx = gamesScrollTop + i;
    if (itemIdx >= GAMES_TOTAL)
      break;

    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (itemIdx == currentGamesSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(gameMenuItems[itemIdx]);
  }

  int barX = 124, barY = 14, barHeight = 34;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_GAMES_ITEMS / GAMES_TOTAL;
  int thumbY = barY + ((barHeight - thumbHeight) * currentGamesSelect /
                       (GAMES_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);

  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.print(F("Back to Menu -> [Back]"));
}

void init2048Game() {
  memset(board2048, 0, sizeof(board2048));
  score2048 = 0;
  gameOver2048 = false;
  spawn2048Tile();
  spawn2048Tile();
}

void spawn2048Tile() {
  int count = 0;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] == 0)
        count++;
    }
  }
  if (count == 0)
    return;
  int target = random(0, count);
  int current = 0;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] == 0) {
        if (current == target) {
          board2048[i][j] = (random(0, 10) == 0) ? 4 : 2;
          return;
        }
        current++;
      }
    }
  }
}

bool check2048GameOver() {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] == 0)
        return false;
      if (i < 3 && board2048[i][j] == board2048[i + 1][j])
        return false;
      if (j < 3 && board2048[i][j] == board2048[i][j + 1])
        return false;
    }
  }
  return true;
}

void handle2048Mode(int vry, int vrx, bool clicked) {
  if (gameOver2048) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(score2048);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }

  bool moved = false;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      for (int i = 0; i < 4; i++) {
        int target = 0;
        for (int j = 0; j < 4; j++) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target++;
          }
        }
        for (int j = 0; j < 3; j++) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i][j + 1]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i][j + 1] = 0;
            moved = true;
          }
        }
        target = 0;
        for (int j = 0; j < 4; j++) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target++;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      for (int i = 0; i < 4; i++) {
        int target = 3;
        for (int j = 3; j >= 0; j--) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target--;
          }
        }
        for (int j = 3; j > 0; j--) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i][j - 1]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i][j - 1] = 0;
            moved = true;
          }
        }
        target = 3;
        for (int j = 3; j >= 0; j--) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target--;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vry < 1000) {
      for (int j = 0; j < 4; j++) {
        int target = 0;
        for (int i = 0; i < 4; i++) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target++;
          }
        }
        for (int i = 0; i < 3; i++) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i + 1][j]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i + 1][j] = 0;
            moved = true;
          }
        }
        target = 0;
        for (int i = 0; i < 4; i++) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target++;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vry > 3000) {
      for (int j = 0; j < 4; j++) {
        int target = 3;
        for (int i = 3; i >= 0; i--) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target--;
          }
        }
        for (int i = 3; i > 0; i--) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i - 1][j]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i - 1][j] = 0;
            moved = true;
          }
        }
        target = 3;
        for (int i = 3; i >= 0; i--) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target--;
          }
        }
      }
      lastJoyAction = millis();
    }
  }

  if (moved) {
    spawn2048Tile();
    if (check2048GameOver())
      gameOver2048 = true;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  display.clearDisplay();
  int startX = 2, startY = 2;
  for (int i = 0; i <= 4; i++) {
    display.drawFastHLine(startX, startY + i * 15, 96, SSD1306_WHITE);
    display.drawFastVLine(startX + i * 24, startY, 60, SSD1306_WHITE);
  }

  display.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] != 0) {
        int val = board2048[i][j];
        int cx = startX + j * 24;
        int cy = startY + i * 15;
        int offset = 9;
        if (val >= 1000)
          offset = 1;
        else if (val >= 100)
          offset = 3;
        else if (val >= 10)
          offset = 6;

        display.setCursor(cx + offset, cy + 4);
        display.print(val);
      }
    }
  }

  display.setCursor(102, 6);
  display.print(F("2048"));
  display.drawFastHLine(100, 16, 26, SSD1306_WHITE);
  display.setCursor(102, 22);
  display.print(F("SCO:"));
  display.setCursor(102, 34);
  display.print(score2048);
}

void initDinoGame() {
  dinoY = 40.0f;
  dinoVelocityY = 0.0f;
  dinoIsJumping = false;
  dinoIsDucking = false;
  scoreDino = 0.0f;
  gameOverDino = false;
  dinoGameSpeed = 0.10f;
  lastDinoUpdate = millis();
  lastObstacleSpawn = 0;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }
}

void handleDinoMode(int vry, int vrx, bool clicked) {
  if (gameOverDino) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println((int)scoreDino);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastDinoUpdate);
  lastDinoUpdate = now;

  if (dt > 50.0f)
    dt = 50.0f;
  if (dt <= 0.0f)
    dt = 1.0f;

  if (vry > 3000) {
    if (!dinoIsJumping) {
      dinoIsDucking = true;
    } else {
      dinoVelocityY += DINO_GRAVITY * 2.5f * dt;
    }
  } else {
    dinoIsDucking = false;
  }

  if (vry < 1000 && !dinoIsJumping && !dinoIsDucking) {
    dinoVelocityY = DINO_JUMP_FORCE;
    dinoIsJumping = true;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  scoreDino += 0.015f * dt;
  dinoGameSpeed += 0.000002f * dt;
  if (dinoGameSpeed > 0.22f)
    dinoGameSpeed = 0.22f;

  if (dinoIsJumping) {
    dinoY += dinoVelocityY * dt;
    dinoVelocityY += DINO_GRAVITY * dt;
    if (dinoY >= 40.0f) {
      dinoY = 40.0f;
      dinoIsJumping = false;
      dinoVelocityY = 0.0f;
    }
  }

  bool obstacleActive = false;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= dinoGameSpeed * dt;
      if (obstacles[i].x < -12.0f) {
        obstacles[i].active = false;
      } else {
        obstacleActive = true;

        int dw = dinoIsDucking ? 14 : 10;
        int dh = dinoIsDucking ? 6 : 12;
        int dy = dinoIsDucking ? 46 : (int)dinoY;
        int dx = 15;

        int ow =
            (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
        int oh =
            (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);

        if (dx < (int)obstacles[i].x + ow && dx + dw > (int)obstacles[i].x &&
            dy < obstacles[i].y + oh && dy + dh > obstacles[i].y) {
          gameOverDino = true;
          return;
        }
      }
    }
  }

  uint32_t spawnInterval = 1500 / (dinoGameSpeed / 0.10f);
  if (spawnInterval < 600)
    spawnInterval = 600;

  if (!obstacleActive && (now - lastObstacleSpawn > spawnInterval)) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) {
        obstacles[i].active = true;
        obstacles[i].x = 128.0f;
        obstacles[i].type = random(0, 3);
        if (obstacles[i].type == 0)
          obstacles[i].y = 42;
        else if (obstacles[i].type == 1)
          obstacles[i].y = 38;
        else
          obstacles[i].y = 32;
        lastObstacleSpawn = now;
        break;
      }
    }
  }

  display.clearDisplay();
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);

  if (dinoIsDucking) {
    display.fillRect(15, 46, 14, 6, SSD1306_WHITE);
  } else {
    display.fillRect(15, (int)dinoY, 10, 12, SSD1306_WHITE);
  }

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int ow =
          (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
      int oh =
          (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);
      if (obstacles[i].type == 2) {
        display.drawRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
      } else {
        display.fillRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
      }
    }
  }

  display.setTextSize(1);
  display.setCursor(90, 2);
  display.print((int)scoreDino);
}

void initBrickGame() {
  brickBallX = 64.0f;
  brickBallY = 45.0f;
  brickBallVX = 1.3f;
  brickBallVY = -1.3f;
  scoreBrick = 0;
  gameOverBrick = false;
  gameWinBrick = false;
  lastBrickUpdate = millis();

  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      brickGrid[r][c] = true;
    }
  }
}

void handleBrickMode(bool clicked) {
  if (gameOverBrick || gameWinBrick) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 15);
    if (gameWinBrick)
      display.println(F("YOU WIN!"));
    else
      display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 38);
    display.print(F("Score: "));
    display.println(scoreBrick);
    display.setCursor(10, 51);
    display.println(F("[Click] Return"));
    return;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  int rawPot = analogRead(POT_PIN);
  float normPot = 1.0f - ((float)rawPot / 4095.0f);
  brickPaddleX = (int)(normPot * (SCREEN_WIDTH - brickPaddleWidth));
  brickPaddleX = constrain(brickPaddleX, 0, SCREEN_WIDTH - brickPaddleWidth);

  if (millis() - lastBrickUpdate > 20) {
    lastBrickUpdate = millis();

    brickBallX += brickBallVX;
    brickBallY += brickBallVY;

    if (brickBallX <= 0) {
      brickBallX = 0;
      brickBallVX = -brickBallVX;
    } else if (brickBallX >= SCREEN_WIDTH - 2) {
      brickBallX = SCREEN_WIDTH - 2;
      brickBallVX = -brickBallVX;
    }

    if (brickBallY <= 10) {
      brickBallY = 10;
      brickBallVY = -brickBallVY;
    }

    if (brickBallY > SCREEN_HEIGHT) {
      gameOverBrick = true;
      return;
    }

    if (brickBallY >= brickPaddleY - 2 && brickBallY <= brickPaddleY + 2) {
      if (brickBallX >= brickPaddleX - 1 &&
          brickBallX <= brickPaddleX + brickPaddleWidth + 1 &&
          brickBallVY > 0) {
        brickBallVY = -brickBallVY;
        float hitCenterOffset =
            (brickBallX - brickPaddleX) / (float)brickPaddleWidth;
        brickBallVX = 3.5f * (hitCenterOffset - 0.5f);
      }
    }

    bool hasBricksLeft = false;
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (brickGrid[r][c]) {
          hasBricksLeft = true;
          int bx = brickOffsetX + c * (brickWidth + brickGapX);
          int by = brickOffsetY + r * (brickHeight + brickGapY);

          if (brickBallX + 2 >= bx && brickBallX <= bx + brickWidth &&
              brickBallY + 2 >= by && brickBallY <= by + brickHeight) {
            brickGrid[r][c] = false;
            brickBallVY = -brickBallVY;
            scoreBrick += 10;

            brickBallVX *= 1.015f;
            brickBallVY *= 1.015f;
            break;
          }
        }
      }
    }

    if (!hasBricksLeft) {
      gameWinBrick = true;
      return;
    }
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("BRICKS"));
  display.setCursor(75, 0);
  display.print(F("SCORE:"));
  display.print(scoreBrick);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      if (brickGrid[r][c]) {
        int bx = brickOffsetX + c * (brickWidth + brickGapX);
        int by = brickOffsetY + r * (brickHeight + brickGapY);
        display.fillRect(bx, by, brickWidth, brickHeight, SSD1306_WHITE);
      }
    }
  }

  display.fillRect(brickPaddleX, brickPaddleY, brickPaddleWidth,
                   brickPaddleHeight, SSD1306_WHITE);

  display.fillRect((int)brickBallX, (int)brickBallY, 2, 2, SSD1306_WHITE);
}

void initStackGame() {
  scoreStack = 0;
  towerLayerCount = 0;
  gameOverStack = false;
  stackBlockWidth = 45;
  stackBlockX = (SCREEN_WIDTH - stackBlockWidth) / 2;
  stackBlockY = 12.0f;
  stackBlockSpeedX = 1.4f;
  stackIsFalling = false;
  cameraViewOffsetY = 0;
  joyMoveLatched = false;

  memset(towerWidths, 0, sizeof(towerWidths));
  memset(towerXs, 0, sizeof(towerXs));

  towerWidths[0] = 50;
  towerXs[0] = (SCREEN_WIDTH - towerWidths[0]) / 2;
  towerLayerCount = 1;
  lastStackTick = millis();
}

void handleStackMode(int vry, int vrx, bool clicked) {
  if (gameOverStack) {
    if (clicked) {
      currentState = STATE_GAMES_MENU;
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(12, 35);
    display.print(F("Layers: "));
    display.println(towerLayerCount - 1);
    display.setCursor(12, 50);
    display.println(F("[Click] return Menu"));
    return;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  bool joyMoved = (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000);

  if (joyMoved) {
    if (!joyMoveLatched && !stackIsFalling) {
      stackIsFalling = true;
      joyMoveLatched = true;
    }
  } else {
    joyMoveLatched = false;
  }

  if (millis() - lastStackTick > 25) {
    lastStackTick = millis();

    if (!stackIsFalling) {
      stackBlockX += stackBlockSpeedX;
      if (stackBlockX <= 0 || stackBlockX + stackBlockWidth >= SCREEN_WIDTH) {
        stackBlockSpeedX = -stackBlockSpeedX;
      }
    } else {
      stackBlockY += 3.5f;

      int targetTopY =
          58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;

      if (stackBlockY >= targetTopY) {
        stackBlockY = targetTopY;
        stackIsFalling = false;

        int baseLeft = towerXs[towerLayerCount - 1];
        int baseRight = baseLeft + towerWidths[towerLayerCount - 1];

        int curLeft = (int)stackBlockX;
        int curRight = curLeft + stackBlockWidth;

        int overlapLeft = max(baseLeft, curLeft);
        int overlapRight = min(baseRight, curRight);

        if (overlapRight > overlapLeft) {
          towerXs[towerLayerCount] = overlapLeft;
          towerWidths[towerLayerCount] = overlapRight - overlapLeft;

          stackBlockWidth = towerWidths[towerLayerCount];

          scoreStack += 10;
          towerLayerCount++;

          if (towerLayerCount >= MAX_STACK_LAYERS) {
            initStackGame();
            return;
          }

          int currentHighestY =
              58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
          if (currentHighestY < 24) {
            cameraViewOffsetY += STACK_BLOCK_HEIGHT;
          }

          stackBlockX = random(0, SCREEN_WIDTH - stackBlockWidth);
          stackBlockY = 12.0f;

          stackBlockSpeedX = (stackBlockSpeedX > 0 ? 1.0f : -1.0f) *
                             (1.2f + (towerLayerCount * 0.12f));
          if (abs(stackBlockSpeedX) > 4.5f)
            stackBlockSpeedX = (stackBlockSpeedX > 0 ? 4.5f : -4.5f);

        } else {
          gameOverStack = true;
        }
      }
    }
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("STACK: "));
  display.print(towerLayerCount - 1);
  display.setCursor(80, 0);
  display.print(F("S:"));
  display.print(scoreStack);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  for (int i = 0; i < towerLayerCount; i++) {
    int drawY = 58 - ((i + 1) * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
    if (drawY > 64)
      continue;
    if (drawY < 10)
      continue;

    display.fillRect(towerXs[i], drawY, towerWidths[i], STACK_BLOCK_HEIGHT - 1,
                     SSD1306_WHITE);
  }

  display.drawRect((int)stackBlockX, (int)stackBlockY, stackBlockWidth,
                   STACK_BLOCK_HEIGHT - 1, SSD1306_WHITE);
}

// --- 科学表达式解析器基础算法 ---
double parseAtom(const char *&p) {
  while (*p == ' ')
    p++;
  if (strncmp(p, "sqrt(", 5) == 0) {
    p += 5;
    double val = parseExpression(p);
    if (*p == ')')
      p++;
    return sqrt(val);
  }
  if (*p == '(') {
    p++;
    double val = parseExpression(p);
    if (*p == ')')
      p++;
    return val;
  }
  char *next;
  double val = strtod(p, &next);
  if (p == next) {
    if (*p != '\0')
      p++;
    return 0;
  }
  p = next;
  return val;
}

double parsePower(const char *&p) {
  double val = parseAtom(p);
  while (*p == ' ')
    p++;
  while (*p == '^') {
    p++;
    val = pow(val, parseAtom(p));
    while (*p == ' ')
      p++;
  }
  return val;
}

double parseTerm(const char *&p) {
  double val = parsePower(p);
  while (*p == ' ')
    p++;
  while (*p == '*' || *p == '/') {
    char op = *p++;
    double next = parsePower(p);
    if (op == '*')
      val *= next;
    else if (next != 0)
      val /= next;
    else
      val = NAN;
    while (*p == ' ')
      p++;
  }
  return val;
}

double parseExpression(const char *&p) {
  double val = parseTerm(p);
  while (*p == ' ')
    p++;
  while (*p == '+' || *p == '-') {
    char op = *p++;
    double next = parseTerm(p);
    if (op == '+')
      val += next;
    else
      val -= next;
    while (*p == ' ')
      p++;
  }
  return val;
}

void handleCalculatorMode(int vry, int vrx, bool clicked) {
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      calcCy = (calcCy == 0) ? 3 : calcCy - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      calcCy = (calcCy == 3) ? 0 : calcCy + 1;
      lastJoyAction = millis();
    }
    if (vrx < 1000) {
      calcCx = (calcCx == 0) ? 5 : calcCx - 1;
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      calcCx = (calcCx == 5) ? 0 : calcCx + 1;
      lastJoyAction = millis();
    }
  }

  if (clicked) {
    char key = calcKeys[calcCy][calcCx];
    if (calcClearOnNext && key != '=' && key != 'D') {
      calcInput[0] = '\0';
      calcClearOnNext = false;
    }

    if (key == 'C') {
      calcInput[0] = '\0';
      calcResultStr[0] = '\0';
      calcClearOnNext = false;
    } else if (key == 'D') {
      int len = strlen(calcInput);
      if (len > 0) {
        if (len >= 5 && strcmp(&calcInput[len - 5], "sqrt(") == 0) {
          calcInput[len - 5] = '\0';
        } else {
          calcInput[len - 1] = '\0';
        }
      }
    } else if (key == '=') {
      if (strlen(calcInput) > 0) {
        const char *p = calcInput;
        double res = parseExpression(p);
        if (isnan(res)) {
          strcpy(calcResultStr, "= Error");
        } else {
          if (res == (long)res) {
            sprintf(calcResultStr, "= %ld", (long)res);
          } else {
            char temp[20];
            dtostrf(res, 12, 4, temp);
            int rLen = strlen(temp);
            while (rLen > 0 && (temp[rLen - 1] == ' ' || temp[rLen - 1] == '0'))
              temp[--rLen] = '\0';
            if (rLen > 0 && temp[rLen - 1] == '.')
              temp[--rLen] = '\0';
            sprintf(calcResultStr, "= %s", temp);
          }
        }
        calcClearOnNext = true;
      }
    } else if (key == 'S') {
      if (strlen(calcInput) + 5 < sizeof(calcInput))
        strcat(calcInput, "sqrt(");
    } else if (key == 'P') {
      if (strlen(calcInput) + 6 < sizeof(calcInput))
        strcat(calcInput, "3.1416");
    } else if (key != ' ') {
      int len = strlen(calcInput);
      if (len + 1 < sizeof(calcInput)) {
        calcInput[len] = key;
        calcInput[len + 1] = '\0';
      }
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(calcInput);
  display.setCursor(2, 11);
  display.print(calcResultStr);
  display.drawFastHLine(0, 22, 128, SSD1306_WHITE);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 6; c++) {
      int x = c * 21 + 1;
      int y = 24 + r * 10 + 1;
      char key = calcKeys[r][c];

      if (r == calcCy && c == calcCx) {
        display.fillRect(x - 1, y - 1, 21, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      display.setCursor(x + 4, y + 1);
      if (key == 'D')
        display.print(F("<-"));
      else if (key == 'S')
        display.print(F("sq"));
      else if (key == 'P')
        display.print(F("pi"));
      else if (key != ' ')
        display.print(key);
    }
  }
  display.setTextColor(SSD1306_WHITE);
}

void handleClockMode(bool clicked) {
  if (clicked) {
    currentState = STATE_MAIN_MENU;
    return;
  }

  RtcDateTime now = Rtc.GetDateTime();
  display.clearDisplay();

  char dateStr[12];
  sprintf(dateStr, "%04d-%02d-%02d", now.Year(), now.Month(), now.Day());
  display.setTextSize(1);
  display.setCursor(34, 10);
  display.println(dateStr);

  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  display.setTextSize(2);
  display.setCursor(16, 26);
  display.println(timeStr);

  display.setTextSize(1);
  display.setCursor(19, 52);
  display.print(F("[Click] to Menu"));
}

void handleSettingsMode(int vry, int vrx, bool clicked) {
  unsigned long nowMs = millis();
  if (nowMs - lastSettingsTick >= 1000) {
    int passedSeconds = (nowMs - lastSettingsTick) / 1000;
    lastSettingsTick += passedSeconds * 1000;

    editSecond += passedSeconds;
    if (editSecond >= 60) {
      editMinute += editSecond / 60;
      editSecond %= 60;
      if (editMinute >= 60) {
        editHour += editMinute / 60;
        editMinute %= 60;
        if (editHour >= 24) {
          editDay += editHour / 24;
          editHour %= 24;
          int maxD = getMaxDay(editYear, editMonth);
          while (editDay > maxD) {
            editDay -= maxD;
            editMonth++;
            if (editMonth > 12) {
              editYear++;
              editMonth = 1;
            }
            maxD = getMaxDay(editYear, editMonth);
          }
        }
      }
    }
  }

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      if (currentEditField == FIELD_MONTH)
        currentEditField = FIELD_YEAR;
      else if (currentEditField == FIELD_DAY)
        currentEditField = FIELD_MONTH;
      else if (currentEditField == FIELD_HOUR)
        currentEditField = FIELD_DAY;
      else if (currentEditField == FIELD_MINUTE)
        currentEditField = FIELD_HOUR;
      else if (currentEditField == FIELD_SECOND)
        currentEditField = FIELD_MINUTE;
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      if (currentEditField == FIELD_YEAR)
        currentEditField = FIELD_MONTH;
      else if (currentEditField == FIELD_MONTH)
        currentEditField = FIELD_DAY;
      else if (currentEditField == FIELD_DAY)
        currentEditField = FIELD_HOUR;
      else if (currentEditField == FIELD_HOUR)
        currentEditField = FIELD_MINUTE;
      else if (currentEditField == FIELD_MINUTE)
        currentEditField = FIELD_SECOND;
      lastJoyAction = millis();
    }

    if (vry < 1000) {
      if (currentEditField == FIELD_YEAR)
        editYear++;
      else if (currentEditField == FIELD_MONTH) {
        editMonth++;
        if (editMonth > 12)
          editMonth = 1;
      } else if (currentEditField == FIELD_DAY) {
        editDay++;
        if (editDay > getMaxDay(editYear, editMonth))
          editDay = 1;
      } else if (currentEditField == FIELD_HOUR) {
        editHour++;
        if (editHour > 23)
          editHour = 0;
      } else if (currentEditField == FIELD_MINUTE) {
        editMinute++;
        if (editMinute > 59)
          editMinute = 0;
      } else if (currentEditField == FIELD_SECOND) {
        editSecond++;
        if (editSecond > 59)
          editSecond = 0;
      }
      lastJoyAction = millis();
    } else if (vry > 3000) {
      if (currentEditField == FIELD_YEAR)
        editYear--;
      else if (currentEditField == FIELD_MONTH) {
        editMonth--;
        if (editMonth < 1)
          editMonth = 12;
      } else if (currentEditField == FIELD_DAY) {
        editDay--;
        if (editDay < 1)
          editDay = getMaxDay(editYear, editMonth);
      } else if (currentEditField == FIELD_HOUR) {
        editHour--;
        if (editHour < 0)
          editHour = 23;
      } else if (currentEditField == FIELD_MINUTE) {
        editMinute--;
        if (editMinute < 0)
          editMinute = 59;
      } else if (currentEditField == FIELD_SECOND) {
        editSecond--;
        if (editSecond < 0)
          editSecond = 59;
      }
      lastJoyAction = millis();
    }

    int maxDaysInCurrentMonth = getMaxDay(editYear, editMonth);
    if (editDay > maxDaysInCurrentMonth) {
      editDay = maxDaysInCurrentMonth;
    }
  }

  if (clicked) {
    RtcDateTime updated(editYear, editMonth, editDay, editHour, editMinute,
                        editSecond);
    Rtc.SetDateTime(updated);
    currentState = STATE_MAIN_MENU;
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("[EDIT] X:Move  Y:+-"));
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  char dateBuf[22];
  char timeBuf[22];
  sprintf(dateBuf, "DATE: %04d-%02d-%02d", editYear, editMonth, editDay);
  sprintf(timeBuf, "TIME: %02d:%02d:%02d", editHour, editMinute, editSecond);

  display.setCursor(10, 22);
  display.print(dateBuf);
  display.setCursor(10, 38);
  display.print(timeBuf);

  if (currentEditField == FIELD_YEAR)
    display.fillRect(46, 31, 24, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_MONTH)
    display.fillRect(76, 31, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_DAY)
    display.fillRect(94, 31, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_HOUR)
    display.fillRect(46, 47, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_MINUTE)
    display.fillRect(64, 47, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_SECOND)
    display.fillRect(82, 47, 12, 2, SSD1306_WHITE);
}

// --- 贪吃蛇游戏模块 ---
void initSnakeGame() {
  snakeLength = 3;
  snake[0] = {15, 8};
  snake[1] = {14, 8};
  snake[2] = {13, 8};
  snakeDir = SNAKE_RIGHT;
  isGameOver = false;
  spawnFood();
}

void spawnFood() {
  food.x = random(0, GRID_WIDTH);
  food.y = random(0, GRID_HEIGHT);
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x == food.x && snake[i].y == food.y) {
      food.x = random(0, GRID_WIDTH);
      food.y = random(0, GRID_HEIGHT);
      i = -1;
    }
  }
}

void handleSnakeMode(int vry, int vrx, bool clicked) {
  if (isGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(snakeLength - 3);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }

  if (vrx < 1000 && snakeDir != SNAKE_RIGHT)
    snakeDir = SNAKE_LEFT;
  else if (vrx > 3000 && snakeDir != SNAKE_LEFT)
    snakeDir = SNAKE_RIGHT;
  else if (vry < 1000 && snakeDir != SNAKE_DOWN)
    snakeDir = SNAKE_UP;
  else if (vry > 3000 && snakeDir != SNAKE_UP)
    snakeDir = SNAKE_DOWN;

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  if (millis() - lastSnakeUpdate > snakeSpeed) {
    lastSnakeUpdate = millis();
    for (int i = snakeLength - 1; i > 0; i--)
      snake[i] = snake[i - 1];

    if (snakeDir == SNAKE_UP)
      snake[0].y--;
    else if (snakeDir == SNAKE_DOWN)
      snake[0].y++;
    else if (snakeDir == SNAKE_LEFT)
      snake[0].x--;
    else if (snakeDir == SNAKE_RIGHT)
      snake[0].x++;

    if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH || snake[0].y < 0 ||
        snake[0].y >= GRID_HEIGHT) {
      isGameOver = true;
      return;
    }
    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        isGameOver = true;
        return;
      }
    }
    if (snake[0].x == food.x && snake[0].y == food.y) {
      if (snakeLength < SNAKE_MAX_LEN)
        snakeLength++;
      spawnFood();
    }
  }

  display.clearDisplay();
  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snake[i].x * SNAKE_BLOCK_SIZE,
                     snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1,
                     SNAKE_BLOCK_SIZE - 1, SSD1306_WHITE);
  }
  display.drawRect(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE,
                   SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE, SSD1306_WHITE);
}

// --- 五子棋配置菜单 ---
void handleGomokuMenu(int vry, int vrx, bool clicked) {
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      gomokuMenuSelect = (gomokuMenuSelect == 0) ? 2 : gomokuMenuSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      gomokuMenuSelect = (gomokuMenuSelect + 1) % 3;
      lastJoyAction = millis();
    }

    if (vrx < 1000 || vrx > 3000) {
      if (gomokuMenuSelect == 0)
        gomokuIsPvE = !gomokuIsPvE;
      if (gomokuMenuSelect == 1 && gomokuIsPvE)
        gomokuDiff = (gomokuDiff == 0) ? 1 : 0;
      lastJoyAction = millis();
    }
  }

  if (clicked) {
    if (gomokuMenuSelect == 2) {
      initGomokuGame();
      currentState = STATE_GOMOKU_PLAY;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== GOMOKU CONFIG ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setCursor(10, 18);
  display.print(gomokuMenuSelect == 0 ? F("> Mode: ") : F("  Mode: "));
  display.println(gomokuIsPvE ? F("VS Computer") : F("Dual Player"));

  display.setCursor(10, 32);
  display.print(gomokuMenuSelect == 1 ? F("> AI:   ") : F("  AI:   "));
  if (gomokuIsPvE)
    display.println(gomokuDiff == 0 ? F("Easy") : F("Hard"));
  else
    display.println(F("N/A"));

  display.setCursor(10, 50);
  display.println(gomokuMenuSelect == 2 ? F("> [ START GAME ]")
                                        : F("  [ START GAME ]"));
}

void initGomokuGame() {
  memset(gomokuBoard, 0, sizeof(gomokuBoard));
  gomokuCx = GOMOKU_SIZE / 2;
  gomokuCy = GOMOKU_SIZE / 2;
  gomokuTurn = 1;
  gomokuWinner = 0;
}

void handleGomokuPlay(int vry, int vrx, bool clicked) {
  if (gomokuWinner != 0) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    if (gomokuWinner == 1)
      display.println(F("P1 WIN!"));
    else if (gomokuWinner == 2)
      display.println(gomokuIsPvE ? F("AI WIN!") : F("P2 WIN!"));
    else
      display.println(F("DRAW GAME"));
    display.setTextSize(1);
    display.setCursor(12, 45);
    display.println(F("[Click] Return Menu"));
    return;
  }

  if (gomokuTurn == 1 || (gomokuTurn == 2 && !gomokuIsPvE)) {
    if (millis() - lastJoyAction > JOY_DELAY) {
      if (vry < 1000 && gomokuCy > 0) {
        gomokuCy--;
        lastJoyAction = millis();
      } else if (vry > 3000 && gomokuCy < GOMOKU_SIZE - 1) {
        gomokuCy++;
        lastJoyAction = millis();
      }
      if (vrx < 1000 && gomokuCx > 0) {
        gomokuCx--;
        lastJoyAction = millis();
      } else if (vrx > 3000 && gomokuCx < GOMOKU_SIZE - 1) {
        gomokuCx++;
        lastJoyAction = millis();
      }
    }

    if (clicked && gomokuBoard[gomokuCx][gomokuCy] == 0) {
      gomokuBoard[gomokuCx][gomokuCy] = gomokuTurn;
      if (checkGomokuWin(gomokuCx, gomokuCy, gomokuTurn)) {
        gomokuWinner = gomokuTurn;
        return;
      }

      bool full = true;
      for (int i = 0; i < GOMOKU_SIZE; i++)
        for (int j = 0; j < GOMOKU_SIZE; j++)
          if (gomokuBoard[i][j] == 0)
            full = false;
      if (full) {
        gomokuWinner = 3;
        return;
      }

      gomokuTurn = (gomokuTurn == 1) ? 2 : 1;
    }
  }

  if (gomokuWinner == 0 && gomokuTurn == 2 && gomokuIsPvE) {
    gomokuAIMove();
    gomokuTurn = 1;
  }

  display.clearDisplay();
  int startX = 4, startY = 5, space = 6;
  for (int i = 0; i < GOMOKU_SIZE; i++) {
    display.drawFastHLine(startX, startY + i * space,
                          (GOMOKU_SIZE - 1) * space + 1, SSD1306_WHITE);
    display.drawFastVLine(startX + i * space, startY,
                          (GOMOKU_SIZE - 1) * space + 1, SSD1306_WHITE);
  }

  for (int x = 0; x < GOMOKU_SIZE; x++) {
    for (int y = 0; y < GOMOKU_SIZE; y++) {
      if (gomokuBoard[x][y] == 1)
        display.fillRect(startX + x * space - 2, startY + y * space - 2, 5, 5,
                         SSD1306_WHITE);
      else if (gomokuBoard[x][y] == 2)
        display.drawRect(startX + x * space - 2, startY + y * space - 2, 5, 5,
                         SSD1306_WHITE);
    }
  }

  display.drawRect(startX + gomokuCx * space - 3, startY + gomokuCy * space - 3,
                   7, 7, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(64, 4);
  display.print(F("GOMOKU"));
  display.drawFastHLine(64, 14, 64, SSD1306_WHITE);
  display.setCursor(64, 20);
  display.print(F("Mode:"));
  display.print(gomokuIsPvE ? F("PvE") : F("PvP"));
  display.setCursor(64, 34);
  display.print(F("Turn:"));
  display.print(gomokuTurn == 1 ? F("P1[X]")
                                : (gomokuIsPvE ? F("AI[O]") : F("P2[O]")));
  display.setCursor(64, 52);
  display.print(F("Exit->Hold"));
}

bool checkGomokuWin(int x, int y, int role) {
  int dx[] = {1, 0, 1, 1};
  int dy[] = {0, 1, 1, -1};
  for (int i = 0; i < 4; i++) {
    int count = 1;
    int tx = x + dx[i], ty = y + dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
           gomokuBoard[tx][ty] == role) {
      count++;
      tx += dx[i];
      ty += dy[i];
    }
    tx = x - dx[i];
    int ty2 = y - dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
           gomokuBoard[tx][ty2] == role) {
      count++;
      tx -= dx[i];
      ty2 -= dy[i];
    }
    if (count >= 5)
      return true;
  }
  return false;
}

void gomokuAIMove() {
  int bestX = -1, bestY = -1, maxScore = -1;
  if (gomokuDiff == 1) {
    for (int x = 0; x < GOMOKU_SIZE; x++) {
      for (int y = 0; y < GOMOKU_SIZE; y++) {
        if (gomokuBoard[x][y] == 0) {
          int totalScore =
              evaluateGomokuPoint(x, y, 2) + evaluateGomokuPoint(x, y, 1) * 1.2;
          if (totalScore > maxScore) {
            maxScore = totalScore;
            bestX = x;
            bestY = y;
          }
        }
      }
    }
  } else {
    for (int x = 0; x < GOMOKU_SIZE; x++)
      for (int y = 0; y < GOMOKU_SIZE; y++)
        if (gomokuBoard[x][y] == 0 && evaluateGomokuPoint(x, y, 2) >= 1000) {
          bestX = x;
          bestY = y;
          goto doMove;
        }
    for (int x = 0; x < GOMOKU_SIZE; x++)
      for (int y = 0; y < GOMOKU_SIZE; y++)
        if (gomokuBoard[x][y] == 0 && evaluateGomokuPoint(x, y, 1) >= 1000) {
          bestX = x;
          bestY = y;
          goto doMove;
        }
    while (true) {
      int rx = random(0, GOMOKU_SIZE), ry = random(0, GOMOKU_SIZE);
      if (gomokuBoard[rx][ry] == 0) {
        bestX = rx;
        bestY = ry;
        break;
      }
    }
  }
doMove:
  if (bestX != -1 && bestY != -1) {
    gomokuBoard[bestX][bestY] = 2;
    gomokuCx = bestX;
    gomokuCy = bestY;
    if (checkGomokuWin(bestX, bestY, 2))
      gomokuWinner = 2;
  }
}

int evaluateGomokuPoint(int x, int y, int role) {
  int dx[] = {1, 0, 1, 1};
  int dy[] = {0, 1, 1, -1};
  int totalWeight = 0;
  for (int i = 0; i < 4; i++) {
    int count = 1, openEnds = 0;
    int tx = x + dx[i], ty = y + dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
           gomokuBoard[tx][ty] == role) {
      count++;
      tx += dx[i];
      ty += dy[i];
    }
    if (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
        gomokuBoard[tx][ty] == 0)
      openEnds++;

    tx = x - dx[i];
    int ty2 = y - dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
           gomokuBoard[tx][ty2] == role) {
      count++;
      tx -= dx[i];
      ty2 -= dy[i];
    }
    if (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
        gomokuBoard[tx][ty2] == 0)
      openEnds++;

    if (count >= 5)
      totalWeight += 5000;
    else if (count == 4 && openEnds == 2)
      totalWeight += 1200;
    else if (count == 4 && openEnds == 1)
      totalWeight += 800;
    else if (count == 3 && openEnds == 2)
      totalWeight += 400;
    else if (count == 3 && openEnds == 1)
      totalWeight += 100;
    else if (count == 2 && openEnds == 2)
      totalWeight += 30;
  }
  return totalWeight;
}

void handleTimersMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) { // 向上
      currentTimersSelect = (currentTimersSelect == 0)
                                ? TIMERS_TOTAL - 1
                                : currentTimersSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) { // 向下
      currentTimersSelect = (currentTimersSelect + 1) % TIMERS_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) { // 向左返回
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) { // 向右确认
      selectTriggered = true;
      lastJoyAction = millis();
    }
  }

  if (selectTriggered) {
    switch (currentTimersSelect) {
    case 0: // 启动正计时
      stopwatchElapsed = 0;
      stopwatchRunning = false;
      currentState = STATE_STOPWATCH;
      break;
    case 1: // 启动倒计时配置
      countdownRunning = false;
      countdownSettingMode = true;
      countdownEditField = 0;
      currentState = STATE_COUNTDOWN;
      break;
    case 2: // 启动番茄钟
      pomoState = POMO_PAUSE;
      pomoPrevState = POMO_WORK;
      pomoRemainingSec = pomoConfigWorkMin * 60;
      currentState = STATE_POMODORO;
      break;
    case 3:
      currentState = STATE_MAIN_MENU;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== TIME TOOLS ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < TIMERS_TOTAL; i++) {
    display.setCursor(5, 16 + (i * 11));
    if (i == currentTimersSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(timerMenuItems[i]);
  }
}

void handleStopwatch(int vry, int vrx, bool clicked) {
  if (clicked) {
    stopwatchRunning = !stopwatchRunning;
    if (stopwatchRunning)
      stopwatchLastTime = millis();
  }

  if (millis() - lastJoyAction > JOY_DELAY && vry > 3000) { // 向下重置
    stopwatchRunning = false;
    stopwatchElapsed = 0;
    lastJoyAction = millis();
  }

  if (stopwatchRunning) {
    unsigned long now = millis();
    stopwatchElapsed += (now - stopwatchLastTime);
    stopwatchLastTime = now;
  }

  // 展开时间要素
  unsigned long totalSec = stopwatchElapsed / 1000;
  int ms100 = (stopwatchElapsed % 1000) / 100; // 百毫秒位
  int sec = totalSec % 60;
  int min = (totalSec / 60) % 60;
  int hour = totalSec / 3600;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("STOPWATCH"));
  display.setCursor(85, 0);
  display.print(stopwatchRunning ? F("RUN") : F("PAUS"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // 绘制大字时间
  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", hour * 60 + min, sec, ms100);
  display.setTextSize(2);
  display.setCursor(16, 25);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("[Click]Paus  [Joy-D]Rst"));
}

void handleCountdown(int vry, int vrx, bool clicked) {
  if (countdownSettingMode) {
    // 设置模式：通过摇杆调整参数
    if (millis() - lastJoyAction > JOY_DELAY) {
      if (vrx < 1000) {
        countdownEditField = 0;
        lastJoyAction = millis();
      } // 选分
      else if (vrx > 3000) {
        countdownEditField = 1;
        lastJoyAction = millis();
      } // 选秒

      int delta = (vry < 1000) ? 1 : ((vry > 3000) ? -1 : 0);
      if (delta != 0) {
        if (countdownEditField == 0) { // 增减分
          countdownTotalSec += delta * 60;
          if (countdownTotalSec < 10)
            countdownTotalSec = 10; // 最低10秒
        } else {                    // 增减秒
          countdownTotalSec += delta;
          if (countdownTotalSec < 10)
            countdownTotalSec = 10;
        }
        countdownTotalSec =
            constrain(countdownTotalSec, 10, 5999); // 限制在99分59秒内
        lastJoyAction = millis();
      }
    }

    if (clicked) {
      countdownRemainingSec = countdownTotalSec;
      countdownRunning = true;
      countdownSettingMode = false;
      countdownStartTime = millis();
    }
  } else {
    // 运行模式
    if (clicked) {
      countdownRunning = !countdownRunning;
      if (countdownRunning)
        countdownStartTime = millis();
    }

    if (countdownRunning) {
      unsigned long now = millis();
      if (now - countdownStartTime >= 1000) {
        long passedSec = (now - countdownStartTime) / 1000;
        countdownRemainingSec -= passedSec;
        countdownStartTime += passedSec * 1000;

        if (countdownRemainingSec <= 0) {
          countdownRemainingSec = 0;
          countdownRunning = false; // 时间到
        }
      }
    }

    // 向下摇杆退出运行状态，回到设置
    if (millis() - lastJoyAction > JOY_DELAY && vry > 3000) {
      countdownRunning = false;
      countdownSettingMode = true;
      lastJoyAction = millis();
    }
  }

  // 渲染
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("COUNTDOWN"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  long dispSec =
      countdownSettingMode ? countdownTotalSec : countdownRemainingSec;
  int m = dispSec / 60;
  int s = dispSec % 60;
  char buf[8];
  sprintf(buf, "%02d:%02d", m, s);

  display.setTextSize(2);
  display.setCursor(34, 25);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(0, 55);
  if (countdownSettingMode) {
    display.print(F("X:Switch Y:+- [Click]Go"));
    // 设定模式下绘制下划线强调高亮选择
    if (countdownEditField == 0)
      display.fillRect(34, 41, 22, 2, SSD1306_WHITE); // 分
    else
      display.fillRect(70, 41, 22, 2, SSD1306_WHITE); // 秒
  } else {
    if (countdownRemainingSec == 0) {
      display.print(F("TIME UP! [Joy-D] Return"));
    } else {
      display.print(countdownRunning ? F("[Click]Paus  [Joy-D]Stop")
                                     : F("[Click]Run   [Joy-D]Stop"));
    }
  }
}

void handlePomodoro(int vry, int vrx, bool clicked) {
  if (clicked) {
    if (pomoState == POMO_PAUSE) {
      pomoState = pomoPrevState; // 恢复恢复
      pomoLastTick = millis();
    } else {
      pomoPrevState = pomoState; // 备份当前是工作还是休息
      pomoState = POMO_PAUSE;
    }
  }

  // 状态机运行时钟滴答
  if (pomoState != POMO_PAUSE) {
    unsigned long now = millis();
    if (now - pomoLastTick >= 1000) {
      long passed = (now - pomoLastTick) / 1000;
      pomoRemainingSec -= passed;
      pomoLastTick += passed * 1000;

      if (pomoRemainingSec <= 0) {
        // 状态流转机制
        if (pomoState == POMO_WORK) {
          pomoCompletedCycles++;
          pomoState = POMO_BREAK;
          pomoRemainingSec = 300; // 短休 5 分钟 = 300秒
        } else {
          pomoState = POMO_WORK;
          pomoRemainingSec = pomoConfigWorkMin * 60; // 重新进入工作
        }
        pomoPrevState = pomoState;
        pomoState = POMO_PAUSE; // 转换周期时强制暂停，提示用户确认切入
      }
    }
  }

  // 渲染
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("POMODORO"));

  display.setCursor(85, 0);
  if (pomoState == POMO_PAUSE) {
    display.print(F("[PAUSE]"));
  } else {
    display.print(pomoState == POMO_WORK ? F("FOCUSED") : F("BREAK"));
  }
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // 绘制剩余倒计时
  int m = pomoRemainingSec / 60;
  int s = pomoRemainingSec % 60;
  char buf[8];
  sprintf(buf, "%02d:%02d", m, s);
  display.setTextSize(2);
  display.setCursor(34, 20);
  display.print(buf);

  // 进度条渲染
  long totalDenom = (pomoState == POMO_BREAK ||
                     (pomoState == POMO_PAUSE && pomoPrevState == POMO_BREAK))
                        ? 300
                        : (pomoConfigWorkMin * 60);
  int barWidth =
      map(constrain(pomoRemainingSec, 0, totalDenom), 0, totalDenom, 0, 100);
  display.drawRect(14, 40, 100, 5, SSD1306_WHITE);
  display.fillRect(14, 40, barWidth, 5, SSD1306_WHITE);

  // 状态统计
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("Done:"));
  display.print(pomoCompletedCycles);
  display.setCursor(60, 55);
  display.print(F("[Click] Start/Paus"));
}
