#include "../Config.h"
#include "../Games.h"
#include "Engine3D.h"

#define SEGMENT_LENGTH 150
#define TOTAL_SEGMENTS 400
#define ROAD_WIDTH 110
#define MAX_CARS 3

struct TrackSegment {
  float curve;
};

static TrackSegment track[TOTAL_SEGMENTS];
static P3DObject enemyCars[MAX_CARS];

static float camZ = 0.0f;
static float playerX =
    0.0f; // 👈 核心重构：自机相对赛道中心线的相对坐标 (-50 到 +50)
static const float camHeight = 32.0f;

static float raceSpeed = 0.0f;
static int raceScore = 0;
static unsigned long lastRaceTick = 0;

void initRacing3D() {
  raceScore = 0;
  raceSpeed = 0.0f;
  camZ = 0.0f;
  playerX = 0.0f;

  // 生成连续多变的急弯赛道
  float currentCurve = 0.0f;
  for (int i = 0; i < TOTAL_SEGMENTS; i++) {
    if (i % 35 == 0)
      currentCurve = (float)random(-25, 26) * 0.12f;
    track[i].curve = (i < 25) ? 0 : currentCurve;
  }

  // 初始化敌机：老老实实锁定分布在 左(-32)、中(0)、右(32) 三条车道上
  for (int i = 0; i < MAX_CARS; i++) {
    enemyCars[i].active = true;
    int lane = random(0, 3);
    enemyCars[i].x = (lane == 0) ? -32.0f : ((lane == 1) ? 0.0f : 32.0f);
    enemyCars[i].z = 400.0f + i * 250.0f;
    enemyCars[i].y = 0.0f;
  }
  lastRaceTick = millis();
}

void handleRacing3DMode(int vry, int vrx, bool clicked) {
  unsigned long now = millis();
  float dt = (float)(now - lastRaceTick) / 25.0f;
  lastRaceTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 1. 电位器左右转向：丝滑操纵相对车道位置
  int rawPot = analogRead(POT_PIN);
  float normPot = 1.0f - ((float)rawPot / 4095.0f);
  playerX = (normPot - 0.5f) * 110.0f; // 约束在马路边界内

  // 2. 自动速度演进
  raceSpeed = constrain(raceSpeed + 0.6f * dt, 0.0f, 16.0f);
  camZ += raceSpeed * dt;
  raceScore += (int)(raceSpeed * 0.12f);

  if (camZ >= TOTAL_SEGMENTS * SEGMENT_LENGTH)
    camZ = 0.0f;

  // 弯道离心力强制修正偏航
  int playerSegIdx = ((int)camZ / SEGMENT_LENGTH) % TOTAL_SEGMENTS;
  playerX -= track[playerSegIdx].curve * (raceSpeed / 16.0f) * 2.2f;
  playerX = constrain(playerX, -55.0f, 55.0f);

  display.clearDisplay();

  int startSeg = (int)(camZ / SEGMENT_LENGTH);
  float fmodCamZ = fmod(camZ, SEGMENT_LENGTH);

  // 3. 🗺️ 渲染 3D 流体赛道网格
  int lastXl = -1, lastXr = -1, lastY = -1;
  float lineCurve = 0.0f;

  for (int i = 0; i < 32; i++) {
    int segIdx = (startSeg + i) % TOTAL_SEGMENTS;
    float relativeZ = i * SEGMENT_LENGTH - fmodCamZ;

    // 弯道曲率累加推演
    lineCurve += track[segIdx].curve * 8.0f;

    int xl, y, scale;
    int xr;
    // 数学坐标系合一：将赛道几何中轴漂移量 lineCurve 直接喂给 3D 空间世界坐标
    project3D(-ROAD_WIDTH + lineCurve, 0, camZ + relativeZ, playerX, camHeight,
              camZ, xl, y, scale);
    project3D(ROAD_WIDTH + lineCurve, 0, camZ + relativeZ, playerX, camHeight,
              camZ, xr, y, scale);

    if (y >= SCREEN_HEIGHT)
      continue;
    if (y <= P3D_HORIZON)
      break;

    if (lastY != -1 && y > lastY) {
      display.drawLine(xl, y, lastXl, lastY, SSD1306_WHITE);
      display.drawLine(xr, y, lastXr, lastY, SSD1306_WHITE);

      // 马路两侧的高速黑白斑马线红白路肩纹理
      if ((startSeg + i) % 2 == 0) {
        display.drawFastHLine(xl, y, 3, SSD1306_WHITE);
        display.drawFastHLine(xr - 3, y, 3, SSD1306_WHITE);
      }
      // 动态滚动的路中心虚线
      if ((startSeg + i) % 4 == 0) {
        int xc = (xl + xr) / 2;
        int lastXc = (lastXl + lastXr) / 2;
        display.drawLine(xc, y, lastXc, lastY, SSD1306_WHITE);
      }
    }
    lastXl = xl;
    lastXr = xr;
    lastY = y;
  }

  // 4. 🚘 渲染并处理敌方车辆
  for (int i = 0; i < MAX_CARS; i++) {
    if (!enemyCars[i].active)
      continue;

    enemyCars[i].z -= 3.2f * dt;
    if (enemyCars[i].z < camZ) {
      enemyCars[i].z = camZ + 700.0f + random(0, 200);
      int lane = random(0, 3);
      enemyCars[i].x = (lane == 0) ? -32.0f : ((lane == 1) ? 0.0f : 32.0f);
    }

    int enemySegIdx = ((int)enemyCars[i].z / SEGMENT_LENGTH) % TOTAL_SEGMENTS;
    float enemyCurveSum = 0.0f;
    for (int s = startSeg; s <= (int)(enemyCars[i].z / SEGMENT_LENGTH); s++) {
      enemyCurveSum += track[s % TOTAL_SEGMENTS].curve * 8.0f;
    }

    int sx, sy, sScale;
    if (project3D(enemyCars[i].x + enemyCurveSum, 0, enemyCars[i].z, playerX,
                  camHeight, camZ, sx, sy, sScale)) {
      int ew = (16 * sScale) / 100;
      int eh = (10 * sScale) / 100;

      if (sx - ew / 2 > 0 && sx + ew / 2 < SCREEN_WIDTH &&
          sy - eh > P3D_HORIZON && sy < SCREEN_HEIGHT) {
        display.drawRect(sx - ew / 2, sy - eh, ew, eh, SSD1306_WHITE);
        display.fillRect(sx - ew / 4, sy - eh + eh / 3, ew / 2, eh / 2,
                         SSD1306_WHITE);
      }

      // 👈【核心修复】：将碰撞判定基准点前移 70
      // 个单位，完美对齐屏幕底部的赛车模型！
      float playerLogicalZ = camZ + 70.0f;

      // 当敌机刚好开到自机的 Z 轴并排区间内（给 24 个单位的纵深车身容差）
      if (abs(enemyCars[i].z - playerLogicalZ) < 24.0f) {
        // 严格比对双方的横向相对车道坐标，车身宽度碰撞压缩至 20
        // 像素，留出完美的擦车超车位
        if (abs((enemyCars[i].x) - playerX) < 20.0f) {
          raceSpeed = 2.0f; // 发生刮擦追尾，强制减速
          raceScore = constrain(raceScore - 40, 0, 99999);
          display.fillScreen(SSD1306_WHITE); // 撞击闪烁反馈
        }
      }
    }
  }

  // 5. 自机底部赛车外形
  int myCarW = 24, myCarH = 12;
  int myCarX = SCREEN_WIDTH / 2 - myCarW / 2;
  int myCarY = SCREEN_HEIGHT - myCarH - 1;
  display.fillRect(myCarX, myCarY, myCarW, myCarH, SSD1306_BLACK);
  display.drawRect(myCarX, myCarY, myCarW, myCarH, SSD1306_WHITE);
  display.drawRect(myCarX + 4, myCarY - 4, 16, 5, SSD1306_WHITE);

  // 数据展示
  display.fillRect(0, 0, 128, 9, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("SCORE:"));
  display.print(raceScore);
  display.setCursor(80, 0);
  display.print(F("SPD:"));
  display.print((int)(raceSpeed * 12));
  display.print(F("km/h"));
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
}
