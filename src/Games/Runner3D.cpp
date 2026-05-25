// src/Games/Runner3D.cpp
#include "../Config.h"
#include "../Games.h"
#include "Engine3D.h"

#define MAX_OBSTACLES 3
#define RUNNER_SPEED 5.2f

struct RunnerObs {
  int8_t lane;
  float z;
  uint8_t type; // 0: 地面实心大铁箱, 1: 悬空大门（带柱子）, 2: 冲天滑道斜坡
  bool active;
};

static RunnerObs obsList[MAX_OBSTACLES];

static int8_t playerLane = 0;
static float playerYOffset = 0;
static float playerGroundY = 0;
static float runnerZ = 0.0f;
static int runScore = 0;
static bool runGameOver = false;

static float jumpVelocity = 0.0f;
static bool isDucking = false;
static unsigned long lastRunTick = 0;

static inline float getLaneX(int8_t lane) { return lane * 42.0f; }

void initRunner3D() {
  runScore = 0;
  runnerZ = 0.0f;
  playerLane = 0;
  playerYOffset = 0.0f;
  playerGroundY = 0.0f;
  jumpVelocity = 0.0f;
  isDucking = false;
  runGameOver = false;

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obsList[i].active = true;
    obsList[i].lane = random(-1, 2);
    obsList[i].z = 500.0f + i * 260.0f;
    obsList[i].type = random(0, 3);
  }
  lastRunTick = millis();
}

void handleRunner3DMode(int vry, int vrx, bool clicked) {
  if (runGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 15);
    display.println(F("CRASHED!"));
    display.setTextSize(1);
    display.setCursor(15, 40);
    display.print(F("Run Score: "));
    display.println(runScore);
    display.setCursor(15, 52);
    display.println(F("[Click] to Exit"));
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastRunTick) / 25.0f;
  lastRunTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 1. 摇杆方向盘切道
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000 && playerLane > -1) {
      playerLane--;
      lastJoyAction = millis();
    } else if (vrx > 3000 && playerLane < 1) {
      playerLane++;
      lastJoyAction = millis();
    }

    if ((vry < 1000 || clicked) && playerYOffset == 0 && !isDucking) {
      jumpVelocity = 4.5f;
      lastJoyAction = millis();
    }
  }

  // 摇杆下拉匍匐伏地
  if (vry > 4000 && playerYOffset == 0) {
    isDucking = true;
    playerYOffset = -8.0f;
  } else if (vry < 2500 && isDucking) {
    isDucking = false;
    playerYOffset = 0.0f;
  }

  // 👈【优化点 2】同步微调重力加速度常数阻尼，消除空中漂浮感
  if (playerYOffset > 0.0f || jumpVelocity > 0.0f) {
    playerYOffset += jumpVelocity * dt;
    jumpVelocity -= 0.42f * dt;
    if (playerYOffset <= 0.0f) {
      playerYOffset = 0.0f;
      jumpVelocity = 0.0f;
    }
  }

  // 斜坡运动高度学爬升
  playerGroundY = 0.0f;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obsList[i].active && obsList[i].type == 2 &&
        obsList[i].lane == playerLane) {
      if (runnerZ >= obsList[i].z && runnerZ <= obsList[i].z + 50.0f) {
        playerGroundY = 20.0f * (runnerZ - obsList[i].z) / 50.0f;
      } else if (runnerZ > obsList[i].z + 50.0f &&
                 runnerZ <= obsList[i].z + 90.0f) {
        playerGroundY = 20.0f;
      }
    }
  }

  runnerZ += RUNNER_SPEED * dt;
  runScore += (int)(1.0f * dt);

  display.clearDisplay();

  // 3. 渲染 3D 透视跑道赛线
  int lastY = -1;
  int lastX[3] = {-1, -1, -1};
  float currentViewCamY = 35.0f + playerGroundY + playerYOffset;

  for (int step = 0; step < 16; step++) {
    float zAnchor = ((int)runnerZ / 80 * 80) + step * 45.0f;
    int y, scale;
    int xL, xC, xR;
    project3D(getLaneX(-1) - 16, 0, zAnchor, getLaneX(playerLane),
              currentViewCamY, runnerZ, xL, y, scale);
    project3D(getLaneX(0), 0, zAnchor, getLaneX(playerLane), currentViewCamY,
              runnerZ, xC, y, scale);
    project3D(getLaneX(1) + 16, 0, zAnchor, getLaneX(playerLane),
              currentViewCamY, runnerZ, xR, y, scale);

    if (y > P3D_HORIZON && y < SCREEN_HEIGHT) {
      if (lastY != -1) {
        display.drawLine(xL, y, lastX[0], lastY, SSD1306_WHITE);
        display.drawLine(xC, y, lastX[1], lastY, SSD1306_WHITE);
        display.drawLine(xR, y, lastX[2], lastY, SSD1306_WHITE);
      }
      lastX[0] = xL;
      lastX[1] = xC;
      lastX[2] = xR;
      lastY = y;
    }
  }

  // 4. 🧱 渲染并更新障碍物方阵
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obsList[i].active)
      continue;

    float zBackCheck = obsList[i].z + ((obsList[i].type == 2) ? 90.0f : 40.0f);

    // 👈【优化点 3：核心改进】障碍物生命周期回收自适应调整
    // 条件 A: 超过车后 100 单位（通用超时）
    // 条件 B: 悬空障碍物(Type 1)只要逻辑 Z 轴越过车体(runnerZ >
    // zBackCheck)，代表成功钻过，立刻原地消失并在远端重刷
    if (obsList[i].z < runnerZ - 100.0f ||
        (obsList[i].type == 1 && runnerZ > zBackCheck)) {
      obsList[i].active = true;
      obsList[i].lane = random(-1, 2);
      obsList[i].z = runnerZ + 700.0f + random(0, 100);
      obsList[i].type = random(0, 3);
      continue;
    }

    float xl = getLaneX(obsList[i].lane) - 18.0f;
    float xr = getLaneX(obsList[i].lane) + 18.0f;

    float yBottom = (obsList[i].type == 1) ? 20.0f : 0.0f;
    float yTop = (obsList[i].type == 1) ? 35.0f : 20.0f;
    float zFront = obsList[i].z;
    float zBack = zBackCheck;

    int fx1, fy1, fs1;
    int fx2, fy2, fs2;
    int fx3, fy3, fs3;
    int fx4, fy4, fs4;
    int bx1, by1, bs1;
    int bx2, by2, bs2;
    int bx3, by3, bs3;
    int bx4, by4, bs4;

    bool fOk1 = project3D(xl, yBottom, zFront, getLaneX(playerLane),
                          currentViewCamY, runnerZ, fx1, fy1, fs1);
    bool fOk2 = project3D(xr, yBottom, zFront, getLaneX(playerLane),
                          currentViewCamY, runnerZ, fx2, fy2, fs2);
    bool fOk3 = project3D(xl, (obsList[i].type == 2 ? 0.0f : yTop), zFront,
                          getLaneX(playerLane), currentViewCamY, runnerZ, fx3,
                          fy3, fs3);
    bool fOk4 = project3D(xr, (obsList[i].type == 2 ? 0.0f : yTop), zFront,
                          getLaneX(playerLane), currentViewCamY, runnerZ, fx4,
                          fy4, fs4);

    bool bOk1 = project3D(xl, yBottom, zBack, getLaneX(playerLane),
                          currentViewCamY, runnerZ, bx1, by1, bs1);
    bool bOk2 = project3D(xr, yBottom, zBack, getLaneX(playerLane),
                          currentViewCamY, runnerZ, bx2, by2, bs2);
    bool bOk3 = project3D(xl, yTop, zBack, getLaneX(playerLane),
                          currentViewCamY, runnerZ, bx3, by3, bs3);
    bool bOk4 = project3D(xr, yTop, zBack, getLaneX(playerLane),
                          currentViewCamY, runnerZ, bx4, by4, bs4);

    // 渲染高对比度前表面
    if (obsList[i].type == 0) {
      // 实心反色白箱子 + 黑色交叉斑马纹
      if (fOk1 && fOk2 && fOk3 && fOk4 && fy1 > P3D_HORIZON) {
        display.fillRect(fx3, fy3, (fx2 - fx1), (fy1 - fy3), SSD1306_WHITE);
        display.drawLine(fx3, fy3, fx2, fy1, SSD1306_BLACK);
        display.drawLine(fx4, fy4, fx1, fy2, SSD1306_BLACK);
      }
    } else if (obsList[i].type == 1) {
      // 悬空招牌 + 垂直扎地的两条侧翼立柱
      if (fOk1 && fOk2 && fOk3 && fOk4 && fy1 > P3D_HORIZON) {
        display.fillRect(fx3, fy3, (fx2 - fx1), (fy1 - fy3), SSD1306_WHITE);
        display.drawFastVLine(fx3, fy3, (fy1 - fy3), SSD1306_WHITE);
        display.drawFastVLine(fx3 + 1, fy3, (fy1 - fy3), SSD1306_WHITE);
        display.drawFastVLine(fx4, fy4, (fy2 - fy4), SSD1306_WHITE);
        display.drawFastVLine(fx4 - 1, fy4, (fy2 - fy4), SSD1306_WHITE);
      }
    } else if (obsList[i].type == 2) {
      // 冲天爬高大斜坡
      if (fOk1 && fOk2 && bOk3 && bOk4 && by3 > P3D_HORIZON) {
        display.drawLine(fx1, fy1, bx3, by3, SSD1306_WHITE);
        display.drawLine(fx2, fy2, bx4, by4, SSD1306_WHITE);
        display.drawLine(bx3, by3, bx4, by4, SSD1306_WHITE);
        display.drawRect(bx3, by3, (bx2 - bx1), (by1 - by3), SSD1306_WHITE);

        for (float f = 0.25f; f < 1.0f; f += 0.25f) {
          int tx1, ty1, ts1;
          int tx2, ty2, ts2;
          project3D(xl, yTop * f, zFront + 50.0f * f, getLaneX(playerLane),
                    currentViewCamY, runnerZ, tx1, ty1, ts1);
          project3D(xr, yTop * f, zFront + 50.0f * f, getLaneX(playerLane),
                    currentViewCamY, runnerZ, tx2, ty2, ts2);
          if (ty1 > P3D_HORIZON && ty1 < SCREEN_HEIGHT)
            display.drawLine(tx1, ty1, tx2, ty1, SSD1306_WHITE);
        }
      }
    }

    // 勾勒 3D 立体中后深度连线
    if (obsList[i].type != 2) {
      if (bOk1 && bOk2 && bOk3 && bOk4 && by1 > P3D_HORIZON)
        display.drawRect(bx3, by3, (bx2 - bx1), (by1 - by3), SSD1306_WHITE);
      if (fOk1 && bOk1 && fy1 > P3D_HORIZON)
        display.drawLine(fx1, fy1, bx1, by1, SSD1306_WHITE);
      if (fOk2 && bOk2 && fy2 > P3D_HORIZON)
        display.drawLine(fx2, fy2, bx2, by2, SSD1306_WHITE);
      if (fOk3 && bOk3 && fy3 > P3D_HORIZON)
        display.drawLine(fx3, fy3, bx3, by3, SSD1306_WHITE);
      if (fOk4 && bOk4 && fy4 > P3D_HORIZON)
        display.drawLine(fx4, fy4, bx4, by4, SSD1306_WHITE);
    }

    // 5. 区间 AABB 全纵深碰撞盒交叉面积判定
    float playerLogicalZ = runnerZ + 74.0f;

    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obsList[i].active)
        continue;

      float zFront = obsList[i].z;
      float zBack = obsList[i].z + ((obsList[i].type == 2) ? 90.0f : 40.0f);

      // 只有当玩家的真实逻辑 Z
      // 轴位置“真正深陷”进入障碍物的纵深区间内时，才会开启判定
      if (playerLogicalZ >= zFront && playerLogicalZ <= zBack) {
        if (obsList[i].lane == playerLane) {
          float currentTotalHeight = playerGroundY + playerYOffset;

          if (obsList[i].type == 0) {
            // 地面大铁箱：如果当前玩家总跳跃高度没能高过箱子顶沿（20单位），判定撞墙
            if (currentTotalHeight < 16.0f) {
              runGameOver = true;
            }
          } else if (obsList[i].type == 1) {
            // 悬空大门：如果玩家总高度没有处于下滑匍匐状态（即高度 >=
            // 0），判定削头
            if (currentTotalHeight >= 0.0f) {
              runGameOver = true;
            }
          } else if (obsList[i].type == 2) {
            // 冲天斜坡：如果玩家在刚触及斜坡边缘的一瞬间按了跳跃导致悬空，判定踢到坡底铲板死亡
            if (playerLogicalZ < zFront + 15.0f && playerYOffset > 4.0f &&
                playerGroundY < 4.0f) {
              runGameOver = true;
            }
          }
        }
      }

      // 成功钻过悬空障碍(Type 1)后，只要车体 Z
      // 轴越过大门后端面，立刻将其就地湮灭回收
      if (obsList[i].type == 1 && playerLogicalZ > zBack) {
        obsList[i].active = true;
        obsList[i].lane = random(-1, 2);
        obsList[i].z = runnerZ + 700.0f + random(0, 100);
        obsList[i].type = random(0, 3);
      }
    }
  }

  // 6. 渲染自机玩家小超人
  int pCenterX = SCREEN_WIDTH / 2;
  int pCenterY = SCREEN_HEIGHT - 12;
  if (isDucking) {
    display.drawRoundRect(pCenterX - 8, pCenterY + 5, 16, 5, 2, SSD1306_WHITE);
  } else {
    display.drawCircle(pCenterX, pCenterY - (int)playerYOffset, 3,
                       SSD1306_WHITE);
    display.drawFastVLine(pCenterX, pCenterY + 3 - (int)playerYOffset, 6,
                          SSD1306_WHITE);
    display.drawFastHLine(pCenterX - 6, pCenterY + 4 - (int)playerYOffset, 13,
                          SSD1306_WHITE);
  }

  // 仪表盘看板
  display.fillRect(0, 0, 128, 9, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("RUN-DIST:"));
  display.print(runScore);
  display.setCursor(84, 0);
  if (playerYOffset > 0.0f)
    display.print(F("JUMPING"));
  else if (isDucking)
    display.print(F("DUCKING"));
  else
    display.print(F("RUNNING"));
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
}
