#ifndef CONFIG_H
#define CONFIG_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <math.h>

// 1. 硬件引脚与物理参数
#define OLED_SDA 32
#define OLED_SCL 25
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define RTC_CLK 4
#define RTC_DAT 16
#define RTC_RST 17

#define JOY_VRX 35
#define JOY_VRY 33
#define JOY_SW 27
#define BTN_BACK 26
#define POT_PIN 34

#define GITHUB_URL "github.com/cubewhy/skid-clock"
#define IDLE_TIMEOUT 180000

// 2. 状态机枚举
enum SystemState {
  STATE_MAIN_MENU,
  STATE_CLOCK,
  STATE_CALCULATOR,
  STATE_HN_READER,
  STATE_GAMES_MENU,
  STATE_SETTINGS,
  STATE_SNAKE,
  STATE_GOMOKU_MENU,
  STATE_GOMOKU_PLAY,
  STATE_2048,
  STATE_DINO,
  STATE_BRICK,
  STATE_STACK,
  STATE_NAVAL_PLAY,
  STATE_PET,
  STATE_TIMERS_MENU,
  STATE_STOPWATCH,
  STATE_COUNTDOWN,
  STATE_POMODORO,
  STATE_WIFI_MENU,
  STATE_WIFI_SCAN,
  STATE_WIFI_KEYBOARD,
  STATE_WIFI_CONNECTING
};

enum PomoState { POMO_WORK, POMO_BREAK, POMO_PAUSE };
enum EditField {
  FIELD_YEAR,
  FIELD_MONTH,
  FIELD_DAY,
  FIELD_HOUR,
  FIELD_MINUTE,
  FIELD_SECOND
};
enum PetState { PET_IDLE, PET_WALK, PET_SLEEP, PET_PETTED };
enum SnakeDirection { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT };

// 3. 数据结构
struct Point {
  int8_t x;
  int8_t y;
};
struct Obstacle {
  float x;
  int16_t y;
  uint8_t type;
  bool active;
};

// 4. 驱动对象 extern 声明
extern Adafruit_SSD1306 display;
extern RtcDS1302<ThreeWire> Rtc;

// 5. 核心状态机 extern 声明
extern SystemState currentState;
extern unsigned long lastActivityTime;
extern unsigned long lastButtonPress;
extern unsigned long lastJoyAction;
extern const int DEBOUNCE_DELAY;
extern const int JOY_DELAY;
extern int currentMenuSelect;
extern int menuScrollTop;
extern const char *menuItems[];
extern const int MENU_TOTAL;
extern const int VISIBLE_ITEMS;
extern bool petEnteredViaTimeout;

// 6. 辅助工具函数
int getMaxDay(int y, int m);

#endif
