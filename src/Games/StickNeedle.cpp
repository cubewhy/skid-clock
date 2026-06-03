#include "Config.h"
#include <cstdint>

#define MAX_NEEDLES 32
#define WHEEL_X 64
#define WHEEL_Y 30
#define WHEEL_RADIUS 10
#define NEEDLE_LENGTH 14

struct PinNeedle {
  float relativeAngle; // 相对核心旋转坐标系的弧度值
  bool active;
};

// 游戏状态变量
static PinNeedle aaNeedles[MAX_NEEDLES];
static uint8_t aaPinnedCount = 0;
static uint8_t aaRemainingNeedles = 15;
static float aaWheelAngle = 0.0f;
static float aaWheelSpeed = 0.04f;

static float aaFlyingY = 56.0f;
static bool aaNeedleFlying = false;
static bool aaFirstFrame = true;
static bool aaFireLocked = false;
static bool aaGameOver = false;
static bool aaGameWin = false;
static unsigned long aaLastTick = 0;

static uint16_t aaLevel = 1;

void initPinGame() {
  aaPinnedCount = 0;

  // 动态计算目标插针数
  aaRemainingNeedles = 8 + (aaLevel > 8 ? 8 : aaLevel);

  aaFirstFrame = true;
  aaFireLocked = false;
  aaWheelAngle = 0.0f;

  // 动态计算初始旋转基础速度
  float baseSpeed = 0.03f + (aaLevel * 0.002f);
  if (baseSpeed > 0.055f)
    baseSpeed = 0.055f;
  aaWheelSpeed = baseSpeed;

  aaFlyingY = 56.0f;
  aaNeedleFlying = false;
  aaGameOver = false;
  aaGameWin = false;
  aaLastTick = millis();

  // 动态生成初始自带的干扰针
  uint8_t prePinnedCount = 2 + (aaLevel % 4);
  if (prePinnedCount + aaRemainingNeedles >= MAX_NEEDLES) {
    prePinnedCount = MAX_NEEDLES - aaRemainingNeedles - 1;
  }

  for (uint8_t i = 0; i < prePinnedCount; i++) {
    float angle = (2.0f * M_PI / prePinnedCount) * i;
    aaNeedles[aaPinnedCount++] = {angle, true};
  }
}

void handlePinGameMode(int vry, int vrx, bool clicked) {
  // 提前计算摇杆是否被推开
  bool joyPushed = (vrx < 1500 || vrx > 2500 || vry < 1500 || vry > 2500);

  if (aaGameOver || aaGameWin) {
    if (!joyPushed) {
      aaFireLocked = false;
    }

    if (clicked || (joyPushed && !aaFireLocked)) {
      if (aaGameWin) {
        aaLevel++;     // 成功通关：关卡数自增
        initPinGame(); // 重新初始化下一关
      } else {
        aaLevel = 1; // 失败：重置关卡回到第一关
        currentState = STATE_GAMES_MENU;
      }
      return;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 10);

    if (aaGameWin) {
      display.println(F("CLEAR!"));
      display.setTextSize(1);
      display.setCursor(15, 35);
      display.print(F("Level "));
      display.print(aaLevel);
      display.print(F(" Passed!"));
      display.setCursor(15, 50);
      display.print(F("[Click/Joy] Next"));
    } else {
      display.println(F("GAME OVER"));
      display.setTextSize(1);
      display.setCursor(15, 35);
      display.print(F("Reached Level: "));
      display.print(aaLevel);
      display.setCursor(15, 50);
      display.print(F("[Click/Joy] Exit"));
    }
    display.display();
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - aaLastTick) / 25.0f;
  aaLastTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 1. 核心轮盘旋转逻辑
  float currentBaseSpeed = 0.03f + (aaLevel * 0.002f);
  if (currentBaseSpeed > 0.055f)
    currentBaseSpeed = 0.055f;

  aaWheelSpeed = (currentBaseSpeed + (aaPinnedCount * 0.003f)) *
                 (aaPinnedCount % 5 == 0 ? -1.0f : 1.0f);
  aaWheelAngle += aaWheelSpeed * dt;

  if (aaWheelAngle > 2.0f * M_PI)
    aaWheelAngle -= 2.0f * M_PI;
  if (aaWheelAngle < 0.0f)
    aaWheelAngle += 2.0f * M_PI;

  // 2. 摇杆发射检测
  if (aaFirstFrame) {
    aaFirstFrame = false;
    aaFireLocked = joyPushed;
  }

  if (joyPushed) {
    if (!aaFireLocked && !aaNeedleFlying) {
      aaNeedleFlying = true;
      aaFlyingY = 56.0f;
      aaFireLocked = true;
    }
  } else {
    aaFireLocked = false;
  }

  // 3. 飞针运动与碰撞判定
  bool showHitFrame = false;
  if (aaNeedleFlying) {
    aaFlyingY -= 4.5f * dt;

    if (aaFlyingY <= (WHEEL_Y + WHEEL_RADIUS)) {
      aaNeedleFlying = false;
      showHitFrame = true; // 记录这一帧刚好撞击

      float hitAngle = M_PI_2 - aaWheelAngle;
      while (hitAngle < 0)
        hitAngle += 2.0f * M_PI;
      while (hitAngle >= 2.0f * M_PI)
        hitAngle -= 2.0f * M_PI;

      bool collision = false;
      for (int i = 0; i < aaPinnedCount; i++) {
        float diff = abs(hitAngle - aaNeedles[i].relativeAngle);
        if (diff > M_PI)
          diff = 2.0f * M_PI - diff;
        if (diff < 0.22f) {
          collision = true;
          break;
        }
      }

      if (collision) {
        aaGameOver = true;
        aaFireLocked = joyPushed;
      } else {
        aaNeedles[aaPinnedCount++] = {hitAngle, true};
        aaRemainingNeedles--;
        if (aaRemainingNeedles == 0) {
          aaGameWin = true;
          aaFireLocked = joyPushed;
        }
      }
    }
  }

  // 4. 图形渲染层
  display.clearDisplay();

  // 顶部状态栏
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("LV:"));
  display.print(aaLevel);
  display.setCursor(85, 0);
  display.print(F("LEFT:"));
  display.print(aaRemainingNeedles);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 绘制中央核心大圆轮
  display.drawCircle(WHEEL_X, WHEEL_Y, WHEEL_RADIUS, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(WHEEL_X - 5, WHEEL_Y - 3);
  display.print(aaRemainingNeedles);

  // 绘制轮盘上所有已扎入的针
  for (int i = 0; i < aaPinnedCount; i++) {
    float currentAbsAngle = aaWheelAngle + aaNeedles[i].relativeAngle;
    int startX = WHEEL_X + (int)(cos(currentAbsAngle) * WHEEL_RADIUS);
    int startY = WHEEL_Y + (int)(sin(currentAbsAngle) * WHEEL_RADIUS);
    int endX =
        WHEEL_X + (int)(cos(currentAbsAngle) * (WHEEL_RADIUS + NEEDLE_LENGTH));
    int endY =
        WHEEL_Y + (int)(sin(currentAbsAngle) * (WHEEL_RADIUS + NEEDLE_LENGTH));

    display.drawLine(startX, startY, endX, endY, SSD1306_WHITE);
    display.fillCircle(endX, endY, 1, SSD1306_WHITE);
  }

  // 绘制发射队列中等待的针/飞行中的针
  if (aaNeedleFlying) {
    display.drawLine(WHEEL_X, (int)aaFlyingY, WHEEL_X,
                     (int)aaFlyingY + NEEDLE_LENGTH, SSD1306_WHITE);
    display.fillCircle(WHEEL_X, (int)aaFlyingY + NEEDLE_LENGTH, 1,
                       SSD1306_WHITE);
  } else if (showHitFrame) {
    display.drawLine(WHEEL_X, WHEEL_Y + WHEEL_RADIUS, WHEEL_X,
                     WHEEL_Y + WHEEL_RADIUS + NEEDLE_LENGTH, SSD1306_WHITE);
    display.fillCircle(WHEEL_X, WHEEL_Y + WHEEL_RADIUS + NEEDLE_LENGTH, 1,
                       SSD1306_WHITE);
  } else if (aaRemainingNeedles > 0) {
    display.drawLine(WHEEL_X, 56, WHEEL_X, 56 + NEEDLE_LENGTH, SSD1306_WHITE);
    display.fillCircle(WHEEL_X, 56 + NEEDLE_LENGTH, 1, SSD1306_WHITE);
  }

  display.display();
}
