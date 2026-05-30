#include "../Config.h"
#include "../Games.h"

static float birdY = 32.0f;
static float birdVel = 0.0f;
static const float fGravity = 0.22f;
static const float flapForce = -2.3f;

static float pipeX = 128.0f;
static int8_t pipeGapY = 24;
static const int8_t pipeGapH = 22;
static const int8_t pipeW = 10;

static int flScore = 0;
static bool flGameOver = false;
static bool joyLatch = false;
static unsigned long lastFlTick = 0;

void initFlappyGame() {
  birdY = 32.0f;
  birdVel = 0.0f;
  pipeX = 128.0f;
  pipeGapY = random(14, 38);
  flScore = 0;
  flGameOver = false;
  joyLatch = false;
  lastFlTick = millis();
}

void handleFlappyMode(int vry, int vrx, bool clicked) {
  if (flGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 12);
    display.println(F("FLAP OVER"));
    display.setTextSize(1);
    display.setCursor(10, 38);
    display.print(F("Score: "));
    display.println(flScore);
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastFlTick) / 25.0f;
  lastFlTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 获取摇杆动态激活标志
  bool joyActive = (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000);

  // 只有在摇杆从中心推向外侧的一瞬间，才判定为单次“翅膀煽动”
  if (joyActive) {
    if (!joyLatch) {
      birdVel = flapForce;
      joyLatch = true; // 锁死
    }
  } else {
    joyLatch = false; // 释放锁
  }

  // 重力环境和位置演进运算
  birdVel += fGravity * dt;
  birdY += birdVel * dt;

  // 水平推进管道
  pipeX -= 1.6f * dt;
  if (pipeX < -pipeW) {
    pipeX = 128.0f;
    pipeGapY = random(14, 38);
    flScore++;
  }

  // 边界碰撞判定
  if (birdY < 10.0f || birdY > 63.0f) {
    flGameOver = true;
  }

  // AABB 管道框体碰撞检测区间扫描
  int bx = 25; // 鸟固定占领的 X 水平轴线
  if (bx + 2 >= (int)pipeX && bx - 2 <= ((int)pipeX + pipeW)) {
    if ((int)birdY - 2 < pipeGapY || (int)birdY + 2 > (pipeGapY + pipeGapH)) {
      flGameOver = true;
    }
  }

  // === 屏幕绘图渲染层 ===
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("FLAPPY:"));
  display.print(flScore);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 绘制上下对称的管道障碍物
  display.fillRect((int)pipeX, 10, pipeW, pipeGapY - 10, SSD1306_WHITE);
  display.fillRect((int)pipeX, pipeGapY + pipeGapH, pipeW,
                   64 - (pipeGapY + pipeGapH), SSD1306_WHITE);

  // 绘制自机小鸟像素外形（2D 十字型单色像素群）
  int iby = (int)birdY;
  display.drawCircle(bx, iby, 2, SSD1306_WHITE);
  display.drawPixel(bx + 1, iby, SSD1306_BLACK); // 鸟眼睛位隔离点点亮
}
