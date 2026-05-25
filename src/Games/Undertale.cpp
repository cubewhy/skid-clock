#include "../Config.h"
#include "../Games.h"
#include <math.h>

#define MAX_UT_BULLETS 16
#define BOX_X 4
#define BOX_Y 16
#define BOX_W 68.0
#define BOX_H 44.0

struct UTBullet {
  float x, y;
  float vx, vy;
  uint8_t type; // 0: 恶念颗粒, 1: 底部升起骨头, 2: 顶部降下骨头
  float param;  // 骨头高度
  bool active;
};

struct GasterBlaster {
  int16_t targetPos;
  bool isVertical;
  uint32_t timer;
  uint8_t stage; // 0: 未激活, 1: 锁定警告, 2: 喷射激光
  bool active;
};

static UTBullet utBullets[MAX_UT_BULLETS];
static GasterBlaster blaster;

static float soulX = 36.0f;
static float soulY = 38.0f;
static int utHP = 92;
static int maxHP = 92;
static uint8_t utWave = 1;
static uint32_t waveStartTime = 0;
static uint32_t lastBulletSpawn = 0;
static uint8_t invulFrames = 0;
static bool utGameOver = false;
static bool utGameWin = false;
static uint32_t lastUtTick = 0;

static const char *utQuotes[5][3] = {
    {"SANS:", "kids", "like you"},    // 第一波
    {"should", "be", "burning."},     // 第二波
    {"BadTime", "is", "coming."},     // 第三波
    {"STAY", "DETER-", "MINED!"},     // 第四波
    {"It's a", "beauti-", "ful day."} // 默认/未定义
};

void initUndertaleGame() {
  soulX = BOX_X + BOX_W / 2;
  soulY = BOX_Y + BOX_H / 2;
  utHP = 92;
  maxHP = 92;
  utWave = 1;
  utGameOver = false;
  utGameWin = false;
  invulFrames = 0;
  waveStartTime = millis();
  lastBulletSpawn = millis();
  lastUtTick = millis();

  for (int i = 0; i < MAX_UT_BULLETS; i++)
    utBullets[i].active = false;
  blaster.active = false;
  blaster.stage = 0;
}

static void drawSoulHeart(int x, int y, bool flash) {
  if (flash && (millis() / 80) % 2 == 0)
    return;
  display.drawPixel(x + 1, y, SSD1306_WHITE);
  display.drawPixel(x + 3, y, SSD1306_WHITE);
  display.fillRect(x, y + 1, 5, 2, SSD1306_WHITE);
  display.fillRect(x + 1, y + 3, 3, 1, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 4, SSD1306_WHITE);
}

void handleUndertaleMode(int vry, int vrx, bool clicked) {
  // === 状态拦截处理器===
  if (utGameOver || utGameWin) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(2);
    display.setCursor(12, 10);

    if (utGameWin) {
      display.println(F("YOU WIN!"));
      display.setTextSize(1);
      display.setCursor(12, 30);
      display.println(F("You saved the")); // 手动断句第 1 行
      display.setCursor(12, 40);
      display.println(F("timeline.")); // 手动断句第 2 行
    } else {
      display.println(F("GAME OVER"));
      display.setTextSize(1);
      display.setCursor(12, 30);
      display.println(F("Don't lose your"));
      display.setCursor(12, 40);
      display.println(F("determination!"));
    }
    display.setCursor(12, 54);
    display.println(F("[Click] Exit"));
    return;
  }

  uint32_t now = millis();
  float dt = (float)(now - lastUtTick) / 25.0f;
  lastUtTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  if (invulFrames > 0)
    invulFrames--;

  // === 1. 玩家红心移动逻辑 ===
  float sSpeed = 1.6f * dt;
  if (vrx < 1000)
    soulX -= sSpeed;
  else if (vrx > 3000)
    soulX += sSpeed;
  if (vry < 1000)
    soulY -= sSpeed;
  else if (vry > 3000)
    soulY += sSpeed;

  soulX = constrain(soulX, BOX_X + 2, BOX_X + BOX_W - 7);
  soulY = constrain(soulY, BOX_Y + 2, BOX_Y + BOX_H - 7);

  // === 2. 波次时间控制 ===
  uint32_t waveElapsed = now - waveStartTime;
  if (waveElapsed > 15000) {
    utWave++;
    waveStartTime = now;
    for (int i = 0; i < MAX_UT_BULLETS; i++)
      utBullets[i].active = false;
    blaster.active = false;
    blaster.stage = 0;
    if (utWave > 4) {
      utGameWin = true;
      return;
    }
  }

  // === 3. 弹幕生成器 ===
  uint32_t spawnRate = constrain(800 - (utWave * 120), 250, 1000);
  if (now - lastBulletSpawn > spawnRate) {
    lastBulletSpawn = now;

    if (utWave == 1 || utWave == 3) {
      for (int i = 0; i < MAX_UT_BULLETS; i++) {
        if (!utBullets[i].active) {
          utBullets[i].active = true;
          utBullets[i].type = random(1, 3);
          utBullets[i].x = BOX_X + BOX_W - 2;
          utBullets[i].vx = -(1.2f + utWave * 0.2f);
          utBullets[i].vy = 0;
          utBullets[i].param = random(12, 24);
          if (utBullets[i].type == 2)
            utBullets[i].y = BOX_Y + 1;
          else
            utBullets[i].y = BOX_Y + BOX_H - 1;
          break;
        }
      }
    }

    if (utWave >= 2) {
      for (int i = 0; i < MAX_UT_BULLETS; i++) {
        if (!utBullets[i].active && random(0, 10) < 4) {
          utBullets[i].active = true;
          utBullets[i].type = 0;
          float angle = (float)random(0, 360) * M_PI / 180.0f;
          utBullets[i].x = soulX + cos(angle) * 45.0f;
          utBullets[i].y = soulY + sin(angle) * 45.0f;
          utBullets[i].vx = -cos(angle) * 1.3f;
          utBullets[i].vy = -sin(angle) * 1.3f;
          break;
        }
      }
    }
  }

  if (utWave >= 3 && !blaster.active && random(0, 100) < 3) {
    blaster.active = true;
    blaster.stage = 1;
    blaster.timer = now;
    blaster.isVertical = (random(0, 2) == 0);
    blaster.targetPos = blaster.isVertical
                            ? random(BOX_X + 10, BOX_X + BOX_W - 10)
                            : random(BOX_Y + 8, BOX_Y + BOX_H - 8);
  }

  // === 4. 物理碰撞检测 ===
  if (blaster.active) {
    if (blaster.stage == 1 && (now - blaster.timer > 700)) {
      blaster.stage = 2;
      blaster.timer = now;
    } else if (blaster.stage == 2 && (now - blaster.timer > 500)) {
      blaster.active = false;
      blaster.stage = 0;
    }

    if (blaster.stage == 2 && invulFrames == 0) {
      bool isHit = false;
      if (blaster.isVertical && (soulX + 2 >= blaster.targetPos - 4 &&
                                 soulX <= blaster.targetPos + 4))
        isHit = true;
      if (!blaster.isVertical && (soulY + 2 >= blaster.targetPos - 4 &&
                                  soulY <= blaster.targetPos + 4))
        isHit = true;

      if (isHit) {
        utHP -= 14;
        invulFrames = 15;
        if (utHP <= 0) {
          utHP = 0;
          utGameOver = true;
        }
      }
    }
  }

  for (int i = 0; i < MAX_UT_BULLETS; i++) {
    if (!utBullets[i].active)
      continue;

    utBullets[i].x += utBullets[i].vx * dt;
    utBullets[i].y += utBullets[i].vy * dt;

    if (utBullets[i].x < BOX_X || utBullets[i].x > BOX_X + BOX_W ||
        utBullets[i].y < BOX_Y || utBullets[i].y > BOX_Y + BOX_H) {
      utBullets[i].active = false;
      continue;
    }

    if (invulFrames == 0) {
      bool collision = false;
      if (utBullets[i].type == 0) {
        if (abs(utBullets[i].x - (soulX + 2)) < 4 &&
            abs(utBullets[i].y - (soulY + 2)) < 4)
          collision = true;
      } else if (utBullets[i].type == 1) {
        if (utBullets[i].x >= soulX - 1 && utBullets[i].x <= soulX + 5) {
          if (soulY + 4 >= BOX_Y + BOX_H - utBullets[i].param)
            collision = true;
        }
      } else if (utBullets[i].type == 2) {
        if (utBullets[i].x >= soulX - 1 && utBullets[i].x <= soulX + 5) {
          if (soulY <= BOX_Y + utBullets[i].param)
            collision = true;
        }
      }

      if (collision) {
        utHP -= 6;
        invulFrames = 12;
        if (utHP <= 0) {
          utHP = 0;
          utGameOver = true;
        }
      }
    }
  }

  // === 5. UI 与视觉图形层渲染 ===
  display.clearDisplay();
  display.setTextWrap(false); // 👈 双重保险：图形渲染阶段彻底关闭自动换行

  // 顶层状态栏展示
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("LV 19"));
  display.setCursor(42, 0);
  display.print(F("HP "));
  display.print(utHP);

  display.drawRect(78, 1, 20, 6, SSD1306_WHITE);
  int hpBarW = map(utHP, 0, maxHP, 0, 18);
  display.fillRect(79, 2, hpBarW, 4, SSD1306_WHITE);

  display.setCursor(104, 0);
  display.print(F("W:"));
  display.print(utWave);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // 绘制中央主战场围框
  display.drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, SSD1306_WHITE);

  // 右侧面板静态框架
  display.setCursor(76, 15);
  display.print(F("* Sans"));
  display.drawFastHLine(76, 24, 50, SSD1306_WHITE);

  int quoteIdx = (utWave <= 4) ? (utWave - 1) : 4;
  for (int l = 0; l < 3; l++) {
    display.setCursor(76,
                      28 + (l * 9)); // 每行垂直间距 9 像素，保证绝不向下溢出
    display.print(utQuotes[quoteIdx][l]);
  }

  // 渲染加斯特巨炮激光
  if (blaster.active) {
    if (blaster.stage == 1) {
      if ((millis() / 50) % 2 == 0) {
        if (blaster.isVertical)
          display.drawFastVLine(blaster.targetPos, BOX_Y + 1, BOX_H - 2,
                                SSD1306_WHITE);
        else
          display.drawFastHLine(BOX_X + 1, blaster.targetPos, BOX_W - 2,
                                SSD1306_WHITE);
      }
    } else if (blaster.stage == 2) {
      if (blaster.isVertical) {
        display.fillRect(blaster.targetPos - 4, BOX_Y + 1, 9, BOX_H - 2,
                         SSD1306_WHITE);
      } else {
        display.fillRect(BOX_X + 1, blaster.targetPos - 4, BOX_W - 2, 9,
                         SSD1306_WHITE);
      }
    }
  }

  // 渲染常规阻击弹幕
  for (int i = 0; i < MAX_UT_BULLETS; i++) {
    if (!utBullets[i].active)
      continue;

    if (utBullets[i].type == 0) {
      int bx = (int)utBullets[i].x;
      int by = (int)utBullets[i].y;
      display.drawPixel(bx, by, SSD1306_WHITE);
      display.drawPixel(bx - 1, by, SSD1306_WHITE);
      display.drawPixel(bx + 1, by, SSD1306_WHITE);
      display.drawPixel(bx, by - 1, SSD1306_WHITE);
      display.drawPixel(bx, by + 1, SSD1306_WHITE);
    } else if (utBullets[i].type == 1) {
      int bx = (int)utBullets[i].x;
      int h = (int)utBullets[i].param;
      int bottomY = BOX_Y + BOX_H - 1;
      display.drawFastVLine(bx, bottomY - h, h, SSD1306_WHITE);
      display.drawFastVLine(bx + 2, bottomY - h, h, SSD1306_WHITE);
      display.drawFastHLine(bx - 1, bottomY - h, 5, SSD1306_WHITE);
    } else if (utBullets[i].type == 2) {
      int bx = (int)utBullets[i].x;
      int h = (int)utBullets[i].param;
      int topY = BOX_Y + 1;
      display.drawFastVLine(bx, topY, h, SSD1306_WHITE);
      display.drawFastVLine(bx + 2, topY, h, SSD1306_WHITE);
      display.drawFastHLine(bx - 1, topY + h, 5, SSD1306_WHITE);
    }
  }

  // 顶层覆盖绘制 SOUL 红心
  drawSoulHeart((int)soulX, (int)soulY, invulFrames > 0);
}
