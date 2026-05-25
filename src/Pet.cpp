#include "Pet.h"
#include "Config.h"

static PetState petState = PET_IDLE;
static int petX = 54;
static int petY = 40;
static int petDir = 1;
static unsigned long lastPetStateChange = 0;
static unsigned long lastPetAnimUpdate = 0;
static int petFrame = 0;
static int lastPetPotVal = 0;

void initPet() {
  petState = PET_IDLE;
  petX = 54;
  petY = 40;
  petFrame = 0;
  lastPetStateChange = millis();
  lastPetAnimUpdate = millis();
  lastPetPotVal = analogRead(POT_PIN);
}

void handlePetMode(int vry, int vrx, bool clicked) {
  if (clicked) {
    currentState = petEnteredViaTimeout ? STATE_CLOCK : STATE_MAIN_MENU;
    return;
  }
  unsigned long now = millis();

  // 1. 解析摇杆方向 (ESP32 ADC 0-4095)
  bool isPushingUp = (vry < 1000); // 必须按住上
  bool isPushingDown = (vry > 3000);
  bool isPushingLeft = (vrx < 1000);
  bool isPushingRight = (vrx > 3000);

  // 2. 核心状态与移动逻辑
  if (isPushingUp) {
    // 【条件：必须按住上】才能触发或维持抓起状态，此时允许任意方向移动
    petState = PET_GRABBED;
    lastPetStateChange = now;

    // 抓起时的移动控制
    if (isPushingLeft)
      petX -= 2;
    if (isPushingRight)
      petX += 2;
    if (isPushingUp)
      petY -= 2; // 继续向上拉
    if (isPushingDown)
      petY += 2;

    // 限制边界
    if (petX < 5)
      petX = 5;
    if (petX > 107)
      petX = 107;
    if (petY < 5)
      petY = 5;
    if (petY > 40)
      petY = 40;

  } else {
    // 【没有按住上】（可能只按了左右、按了下、或者什么都没按）
    if (petY < 40) {
      // 如果此时悬在半空中 -> 进入自由落体
      petState = PET_FALLING;
    } else {
      // 如果已经在地面上 -> 如果刚刚是从抓起/坠落恢复的，切回 IDLE
      if (petState == PET_GRABBED || petState == PET_FALLING) {
        petState = PET_IDLE;
        lastPetStateChange = now;
      }
    }
  }

  // 3. 物理重力：只要不是被抓起状态，且猫咪在空中，就强行下落
  if (petState != PET_GRABBED && petY < 40) {
    petY += 4; // 下落速度
    if (petY >= 40) {
      petY = 40;
      petState = PET_IDLE;
      lastPetStateChange = now;
    }
  }

  // 4. 摸头逻辑 (只有在地面日常状态下才能触发)
  int currentPotVal = analogRead(POT_PIN);
  if (abs(currentPotVal - lastPetPotVal) > 80 && petState != PET_GRABBED &&
      petState != PET_FALLING) {
    petState = PET_PETTED;
    lastPetStateChange = now;
    lastPetPotVal = currentPotVal;
  }
  if (petState == PET_PETTED && (now - lastPetStateChange > 1500)) {
    petState = PET_IDLE;
    lastPetStateChange = now;
  }

  // 5. 随机日常状态分配 (睡觉、走路、发呆)
  if (petState != PET_PETTED && petState != PET_GRABBED &&
      petState != PET_FALLING && (now - lastPetStateChange > 5000)) {
    lastPetStateChange = now;
    int r = random(0, 3);
    if (r == 0)
      petState = PET_IDLE;
    else if (r == 1) {
      petState = PET_WALK;
      petDir = (random(0, 2) == 0) ? 1 : -1;
    } else
      petState = PET_SLEEP;
  }

  // 6. 动画帧更新
  int animInterval = (petState == PET_WALK || petState == PET_GRABBED ||
                      petState == PET_FALLING)
                         ? 140
                         : 350;
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

  // 7. 基础 UI 渲染
  display.clearDisplay();
  display.drawFastHLine(0, 56, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);

  if (petState == PET_PETTED)
    display.print(F("So Comfortable~"));
  else if (petState == PET_GRABBED)
    display.print(F("Put me down!"));
  else if (petState == PET_FALLING)
    display.print(F("Ahhhh~"));
  else
    display.print(F("Cat Life~"));

  RtcDateTime rtcNow = Rtc.GetDateTime();
  char petTimeStr[9];
  sprintf(petTimeStr, "%02d:%02d:%02d", rtcNow.Hour(), rtcNow.Minute(),
          rtcNow.Second());
  display.setCursor(80, 2);
  display.print(petTimeStr);

  int x = petX;
  int y = petY;

  // 8. 猫咪绘图渲染逻辑
  if (petState == PET_GRABBED || petState == PET_FALLING) {
    // 抓起/坠落动画：身体拉长，正面抗议豆豆眼，四肢和尾巴乱抓
    display.fillRoundRect(x + 3, y + 3, 9, 12, 3, SSD1306_WHITE); // 竖直身体
    int hx = x + 3;
    int hy = y - 3;
    display.fillRoundRect(hx, hy, 9, 8, 2, SSD1306_WHITE); // 正面头部

    // 竖起的耳朵
    display.fillTriangle(hx, hy, hx + 2, hy - 3, hx + 3, hy, SSD1306_BLACK);
    display.drawTriangle(hx, hy, hx + 2, hy - 3, hx + 3, hy, SSD1306_WHITE);
    display.fillTriangle(hx + 6, hy, hx + 7, hy - 3, hx + 9, hy, SSD1306_BLACK);
    display.drawTriangle(hx + 6, hy, hx + 7, hy - 3, hx + 9, hy, SSD1306_WHITE);

    // 豆豆眼与小嘴
    display.drawPixel(hx + 2, hy + 3, SSD1306_BLACK);
    display.drawPixel(hx + 6, hy + 3, SSD1306_BLACK);
    display.drawPixel(hx + 4, hy + 5, SSD1306_BLACK);

    // 悬空扑腾的双腿
    int legWiggle = (petFrame % 2 == 0) ? 4 : 2;
    display.drawFastVLine(x + 5, y + 15, legWiggle, SSD1306_WHITE);
    display.drawFastVLine(x + 9, y + 15, 6 - legWiggle, SSD1306_WHITE);

    // 挣扎的双手
    int armOffset = (petState == PET_FALLING && petFrame % 2 == 0) ? 1 : 0;
    display.drawLine(x + 2, y + 6 - armOffset, x, y + 10 - armOffset,
                     SSD1306_WHITE);
    display.drawLine(x + 12, y + 6 - armOffset, x + 14, y + 10 - armOffset,
                     SSD1306_WHITE);

    // 绷直摆动的小尾巴
    int tailWiggle = (petFrame % 2 == 0) ? 1 : -1;
    display.drawLine(x + 7, y + 15, x + 7 + tailWiggle, y + 19, SSD1306_WHITE);

  } else if (petState == PET_SLEEP) {
    // 原始睡眠动画保持不变
    display.setTextWrap(false);
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
    display.setTextWrap(true);
  } else if (petState == PET_PETTED) {
    // 原始抚摸动画保持不变
    display.fillRoundRect(x + 2, y + 4, 12, 9, 3, SSD1306_WHITE);
    int hx = x + 3;
    display.fillRoundRect(hx, y - 1, 9, 8, 2, SSD1306_WHITE);
    display.fillTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                         SSD1306_BLACK);
    display.drawTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                         SSD1306_WHITE);
    display.fillTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                         SSD1306_BLACK);
    display.drawTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                         SSD1306_WHITE);
    display.drawPixel(hx + 2, y + 2, SSD1306_BLACK);
    display.drawPixel(hx + 6, y + 2, SSD1306_BLACK);
    display.drawPixel(hx + 4, y + 4, SSD1306_BLACK);
    display.drawFastVLine(x + 4, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 6, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 10, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 12, y + 13, 3, SSD1306_WHITE);
    int tailWiggle = (petFrame % 2 == 0) ? -3 : 0;
    display.drawLine(x + 14, y + 6, x + 16, y + 2 + tailWiggle, SSD1306_WHITE);
    if (petFrame % 2 == 0) {
      display.setCursor(hx + 1, y - 12);
      display.print(F("<3"));
    } else {
      display.drawFastHLine(hx + 1, y - 9, 6, SSD1306_WHITE);
    }
  } else {
    // 原始闲置/行走动画保持不变
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
