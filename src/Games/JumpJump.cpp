#include "../Config.h"
#include "../Games.h"
#if __has_include("Options.h")
#include "Options.h"
#define HAS_CUSTOM_OPTIONS 1
#else
#define HAS_CUSTOM_OPTIONS 0
#endif

enum JJState { JJ_IDLE, JJ_CHARGE, JJ_FLIGHT, JJ_SCROLL, JJ_GAMEOVER };
static JJState jjState = JJ_IDLE;

static float pX = 20.0f;
static float pY = 50.0f;
static float jumpStartX = 0.0f;
static float jumpTargetX = 0.0f;
static float flightProgress = 0.0f;

static int8_t curPlatX = 10;
static int8_t curPlatW = 20;
static int8_t nextPlatX = 60;
static int8_t nextPlatW = 15;

static float chargePower = 0.0f;
static const float maxCharge = 60.0f;
static int jjScore = 0;
static unsigned long lastJjTick = 0;

static bool inputLocked = true;

void initJumpJumpGame() {
  jjScore = 0;
  jjState = JJ_IDLE;
  pX = 20.0f;
  pY = 50.0f;
  curPlatX = 10;
  curPlatW = 20;
  nextPlatX = random(55, 90);
  nextPlatW = random(12, 22);
  chargePower = 0.0f;
  lastJjTick = millis();
  inputLocked = true;
}

void handleJumpJumpMode(int vry, int vrx, bool clicked) {
  if (jjState == JJ_GAMEOVER) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 15);
    display.println(F("FALL DOWN"));
    display.setTextSize(1);
    display.setCursor(15, 40);
    display.print(F("Score: "));
    display.println(jjScore);
    display.setCursor(15, 53);
    display.print("[Click] to Return");
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastJjTick) / 25.0f;
  lastJjTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 检测摇杆是否离开中心（任意方向偏离均视为动作输入）
  bool joyActive = (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000);

  // 摇杆锁处理：若摇杆未回到中心，则不解除锁定，并忽略当前输入
  if (inputLocked) {
    if (!joyActive) {
      inputLocked = false; // 摇杆回到中心，解锁
    }
    joyActive = false; // 锁定期间强制不触发输入
  }

  switch (jjState) {
  case JJ_IDLE:
    if (joyActive) {
      jjState = JJ_CHARGE;
      chargePower = 0.0f;
    }
    break;

  case JJ_CHARGE:
    if (joyActive) {
      chargePower += 1.8f * dt;
      if (chargePower > maxCharge)
        chargePower = maxCharge;
    } else {
      // 松开摇杆，立即触发起跳流程
      jjState = JJ_FLIGHT;
      jumpStartX = pX;
      jumpTargetX = pX + chargePower * 1.3f;
      flightProgress = 0.0f;
    }
    break;

  case JJ_FLIGHT:
    flightProgress += 0.06f * dt;
    if (flightProgress >= 1.0f) {
      flightProgress = 1.0f;
      pX = jumpTargetX;
      pY = 50.0f;

      // 判定落点
      if (pX >= nextPlatX && pX <= (nextPlatX + nextPlatW)) {
        // 踩到新平台
        jjScore++;
        jjState = JJ_SCROLL;
      } else if (pX >= curPlatX && pX <= (curPlatX + curPlatW)) {
        // 踩回原平台，不计分，返回空闲状态
        jjState = JJ_IDLE;
      } else {
        // 未踩到任何平台
        jjState = JJ_GAMEOVER;
      }
    } else {
      // 抛物线轨迹运动模拟
      pX = jumpStartX + (jumpTargetX - jumpStartX) * flightProgress;
      pY = 50.0f - sin(flightProgress * M_PI) * 22.0f;
    }
    break;

  case JJ_GAMEOVER:
    break;

  case JJ_SCROLL:
    // 整个视角向左滚动，为生成下一个平台腾出空间
    float scrollSpeed = 3.0f * dt;
    pX -= scrollSpeed;
    curPlatX -= scrollSpeed;
    nextPlatX -= scrollSpeed;

    if (nextPlatX <= 20) {
      // 滚动对齐完毕，置换平台状态
      float diff = 20 - nextPlatX;
      pX += diff;
      nextPlatX = 20;
      curPlatX = nextPlatX;
      curPlatW = nextPlatW;

      // 在右侧远端刷新一个新平台
      nextPlatX = random(60, 100);
      nextPlatW = random(12, 22);
      jjState = JJ_IDLE;
    }
    break;
  }

  // === 图形渲染表现层 ===
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("Score:"));
  display.print(jjScore);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 绘制当前的两个地面平台
  display.fillRect(curPlatX, 52, curPlatW, 12, SSD1306_WHITE);
  display.fillRect(nextPlatX, 52, nextPlatW, 12, SSD1306_WHITE);

  // 绘制蓄力进度条与轨迹辅助线
  if (jjState == JJ_CHARGE) {
    // 1. 蓄力进度条
    int barWidth = (int)((chargePower / maxCharge) * 40);
    int barStartX = 60;
    display.drawRect(barStartX, 2, 42, 5, SSD1306_WHITE);
    display.fillRect(barStartX + 1, 3, barWidth, 3, SSD1306_WHITE);

#if JJ_ENABLE_PROJ
    // 2. 预估轨迹辅助虚线
    float projTargetX = pX + chargePower * 1.3f;
    for (float t = 0.15f; t < 1.0f; t += 0.15f) {
      float projX = pX + (projTargetX - pX) * t;
      float projY = 50.0f - sin(t * M_PI) * 22.0f;
      display.drawPixel((int)projX, (int)projY, SSD1306_WHITE);
    }
#endif // ENABLE_PROJ
  }

  // 绘制跳跃小主体（3x3 像素实心小方块）
  display.fillRect((int)pX - 1, (int)pY - 3, 3, 3, SSD1306_WHITE);
}
