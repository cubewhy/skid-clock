#include "../Config.h"
#include "../Games.h"
#include <math.h>

#define MAX_TRBL_BULLETS 8
#define MAX_TRBL_ENEMIES 1
#define PLAY_ZONE_Y 10
#define MAX_WALLS 14

struct TroubleBullet {
  float x, y;
  float vx, vy;
  int8_t bouncesLeft;
  bool fromEnemy;
  bool active;
};

struct TroubleEnemy {
  float x, y;
  float targetX, targetY;
  float turretAngle;
  float sweepDir; // AI 炮塔自转扫射方向 (1.0f 或 -1.0f)
  unsigned long lastShot;
  float lastX, lastY; // 用于防卡死判定
  uint8_t stuckFrames;
  bool active;
};

struct MazeWall {
  int16_t x1, y1, x2, y2;
  bool isVertical;
};

static MazeWall activeMaze[MAX_WALLS];
static uint8_t currentTotalWalls = 0;

static TroubleBullet tkBullets[MAX_TRBL_BULLETS];
static TroubleEnemy tkEnemy;

// 👈 优化：将出生点移至完全远离隔断墙的绝对几何安全口袋阵中
static float playerX = 11.0f;
static float playerY = 52.0f;
static int tkScore = 0;
static bool tkGameOver = false;

static unsigned long lastPlayerFire = 0;
static unsigned long lastTkTick = 0;

// 光线投射射线视线遮挡算法
static bool checkLineOfSight(float ax, float ay, float bx, float by) {
  for (int w = 0; w < currentTotalWalls; w++) {
    MazeWall wall = activeMaze[w];
    if (wall.isVertical) {
      if ((ax < wall.x1 && bx > wall.x1) || (ax > wall.x1 && bx < wall.x1)) {
        float t = (wall.x1 - ax) / (bx - ax);
        float intersectY = ay + t * (by - ay);
        if (intersectY >= wall.y1 && intersectY <= wall.y2)
          return false;
      }
    } else {
      if ((ay < wall.y1 && by > wall.y1) || (ay > wall.y1 && by < wall.y1)) {
        float t = (wall.y1 - ay) / (by - ay);
        float intersectX = ax + t * (bx - ax);
        if (intersectX >= wall.x1 && intersectX <= wall.x2)
          return false;
      }
    }
  }
  return true;
}

// 拓扑对称绝对通畅迷宫生成器
static void generateRandomMaze() {
  currentTotalWalls = 0;

  // 1. 注入 4 面边界大围墙
  activeMaze[currentTotalWalls++] = {0, 10, 127, 10, false};
  activeMaze[currentTotalWalls++] = {0, 63, 127, 63, false};
  activeMaze[currentTotalWalls++] = {0, 10, 0, 63, true};
  activeMaze[currentTotalWalls++] = {127, 10, 127, 63, true};

  // 2. 加载高精复杂的隔断墙阵列
  uint8_t style = random(0, 3);
  if (style == 0) {
    // 👈【已修复】：彻底修正原有手滑敲错变量导致的堆栈越界踩踏 Bug
    activeMaze[currentTotalWalls++] = {28, 10, 28, 32, true};
    activeMaze[currentTotalWalls++] = {28, 32, 48, 32, false};
    activeMaze[currentTotalWalls++] = {100, 10, 100, 32, true};
    activeMaze[currentTotalWalls++] = {80, 32, 100, 32, false};
    activeMaze[currentTotalWalls++] = {28, 44, 28, 63, true};
    activeMaze[currentTotalWalls++] = {100, 44, 100, 63, true};
    activeMaze[currentTotalWalls++] = {48, 48, 80, 48, false};
  } else if (style == 1) {
    activeMaze[currentTotalWalls++] = {24, 24, 54, 24, false};
    activeMaze[currentTotalWalls++] = {74, 24, 104, 24, false};
    activeMaze[currentTotalWalls++] = {24, 50, 54, 50, false};
    activeMaze[currentTotalWalls++] = {74, 50, 104, 50, false};
    activeMaze[currentTotalWalls++] = {40, 24, 40, 50, true};
    activeMaze[currentTotalWalls++] = {88, 24, 88, 50, true};
  } else {
    activeMaze[currentTotalWalls++] = {22, 10, 22, 46, true};
    activeMaze[currentTotalWalls++] = {106, 10, 106, 46, true};
    activeMaze[currentTotalWalls++] = {46, 26, 82, 26, false};
    activeMaze[currentTotalWalls++] = {46, 48, 82, 48, false};
    activeMaze[currentTotalWalls++] = {64, 34, 64, 48, true};
  }

  // 3. 随机置入微观反弹小障碍立柱
  if (random(0, 100) < 65)
    activeMaze[currentTotalWalls++] = {64, 15, 64, 21, true};
  if (random(0, 100) < 65)
    activeMaze[currentTotalWalls++] = {52, 37, 58, 37, false};
}

static void respawnEnemy() {
  tkEnemy.active = true;
  tkEnemy.lastShot = millis();
  tkEnemy.turretAngle = 0.0f;
  tkEnemy.sweepDir = 1.0f;
  tkEnemy.stuckFrames = 0;

  // 永远降生在离玩家最远的对角线黄金口袋区 (X=116, Y=20 绝对安全)
  tkEnemy.x = (playerX > 64.0f) ? 11.0f : 116.0f;
  tkEnemy.y = 20.0f;
  tkEnemy.lastX = tkEnemy.x;
  tkEnemy.lastY = tkEnemy.y;

  tkEnemy.targetX = random(20, 110);
  tkEnemy.targetY = random(18, 56);
}

void initTankTroubleGame() {
  playerX = 11.0f;
  playerY = 52.0f;
  tkScore = 0;
  tkGameOver = false;

  for (int i = 0; i < MAX_TRBL_BULLETS; i++)
    tkBullets[i].active = false;

  generateRandomMaze();
  respawnEnemy();

  lastPlayerFire = millis();
  lastTkTick = millis();
}

void handleTankTroubleMode(int vry, int vrx, bool clicked) {
  if (tkGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 14);
    display.println(F("YOU DIED"));
    display.setTextSize(1);
    display.setCursor(10, 38);
    display.print(F("Your Score: "));
    display.print(tkScore);
    display.setCursor(10, 51);
    display.println(F("[Click] Replay"));
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastTkTick) / 25.0f;
  lastTkTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 1. 玩家移动与 activeMaze 墙体裁剪阻挡
  float moveSpeed = 1.2f * dt;
  float nextPX = playerX;
  float nextPY = playerY;

  if (vrx < 1000)
    nextPX -= moveSpeed;
  else if (vrx > 3000)
    nextPX += moveSpeed;
  if (vry < 1000)
    nextPY -= moveSpeed;
  else if (vry > 3000)
    nextPY += moveSpeed;

  for (int w = 0; w < currentTotalWalls; w++) {
    if (activeMaze[w].isVertical) {
      if (nextPY >= activeMaze[w].y1 - 4 && nextPY <= activeMaze[w].y2 + 4) {
        if (abs(nextPX - activeMaze[w].x1) < 5)
          nextPX = playerX;
      }
    } else {
      if (nextPX >= activeMaze[w].x1 - 4 && nextPX <= activeMaze[w].x2 + 4) {
        if (abs(nextPY - activeMaze[w].y1) < 5)
          nextPY = playerY;
      }
    }
  }
  playerX = nextPX;
  playerY = nextPY;

  // 2. 旋钮控自机炮塔
  int rawPot = analogRead(POT_PIN);
  float aimAngle = ((float)rawPot / 4095.0f) * 2.0f * M_PI;

  // 3. 玩家击发子弹
  if (clicked && (now - lastPlayerFire > 400)) {
    lastPlayerFire = now;
    for (int i = 0; i < MAX_TRBL_BULLETS; i++) {
      if (!tkBullets[i].active) {
        tkBullets[i].active = true;
        tkBullets[i].bouncesLeft = 5;
        tkBullets[i].fromEnemy = false; // 👈 标记：玩家子弹
        tkBullets[i].x = playerX + cos(aimAngle) * 6.0f;
        tkBullets[i].y = playerY + sin(aimAngle) * 6.0f;
        tkBullets[i].vx = cos(aimAngle) * 2.6f;
        tkBullets[i].vy = sin(aimAngle) * 2.6f;
        break;
      }
    }
  }

  // 4. 子弹流体反弹与硬核伤害链判定
  for (int i = 0; i < MAX_TRBL_BULLETS; i++) {
    if (!tkBullets[i].active)
      continue;

    float bNextX = tkBullets[i].x + tkBullets[i].vx * dt;
    float bNextY = tkBullets[i].y + tkBullets[i].vy * dt;

    for (int w = 0; w < currentTotalWalls; w++) {
      if (activeMaze[w].isVertical) {
        if (tkBullets[i].y >= activeMaze[w].y1 &&
            tkBullets[i].y <= activeMaze[w].y2) {
          if ((tkBullets[i].x <= activeMaze[w].x1 &&
               bNextX >= activeMaze[w].x1 && tkBullets[i].vx > 0) ||
              (tkBullets[i].x >= activeMaze[w].x1 &&
               bNextX <= activeMaze[w].x1 && tkBullets[i].vx < 0)) {
            tkBullets[i].vx = -tkBullets[i].vx;
            bNextX = tkBullets[i].x + (tkBullets[i].vx > 0 ? 1.0f : -1.0f);
            tkBullets[i].bouncesLeft--;
          }
        }
      } else {
        if (activeMaze[w].x1 <= tkBullets[i].x &&
            tkBullets[i].x <= activeMaze[w].x2) {
          if ((tkBullets[i].y <= activeMaze[w].y1 &&
               bNextY >= activeMaze[w].y1 && tkBullets[i].vy > 0) ||
              (tkBullets[i].y >= activeMaze[w].y1 &&
               bNextY <= activeMaze[w].y1 && tkBullets[i].vy < 0)) {
            tkBullets[i].vy = -tkBullets[i].vy;
            bNextY = tkBullets[i].y + (tkBullets[i].vy > 0 ? 1.0f : -1.0f);
            tkBullets[i].bouncesLeft--;
          }
        }
      }
    }

    tkBullets[i].x = bNextX;
    tkBullets[i].y = bNextY;

    if (tkBullets[i].bouncesLeft <= 0 || tkBullets[i].x < 2 ||
        tkBullets[i].x > 126 || tkBullets[i].y < 11 || tkBullets[i].y > 62) {
      tkBullets[i].active = false;
      continue;
    }

    // --- ✨【核心碰撞逻辑重构：首弹防贴脸伤害机制】---

    // A.
    // 击中自机玩家：只有在（子弹是敌人发出来的）或者（自己发出来的子弹至少反弹过一次，即剩余次数小于5）时才触发暴毙
    if (tkBullets[i].fromEnemy || tkBullets[i].bouncesLeft < 5) {
      float dToPlayer =
          pow(tkBullets[i].x - playerX, 2) + pow(tkBullets[i].y - playerY, 2);
      if (dToPlayer < 18.0f) {
        tkBullets[i].active = false;
        tkGameOver = true;
        return;
      }
    }

    // B. 击中敌人 AI：同理，如果是 AI 自己打出来的子弹且没有反弹过，免伤放行！
    if (tkEnemy.active) {
      if (!tkBullets[i].fromEnemy || tkBullets[i].bouncesLeft < 5) {
        float dToEnemy = pow(tkBullets[i].x - tkEnemy.x, 2) +
                         pow(tkBullets[i].y - tkEnemy.y, 2);
        if (dToEnemy < 18.0f) {
          tkBullets[i].active = false;
          tkEnemy.active = false;
          tkScore++;
          respawnEnemy();
          break;
        }
      }
    }
  }

  // 5. 🤖 战术型人机 AI 驱动状态机
  if (tkEnemy.active) {
    bool seePlayer = checkLineOfSight(tkEnemy.x, tkEnemy.y, playerX, playerY);

    // AI 贴墙拐角防粘连卡死
    if (abs(tkEnemy.x - tkEnemy.lastX) < 0.04f &&
        abs(tkEnemy.y - tkEnemy.lastY) < 0.04f) {
      tkEnemy.stuckFrames++;
      if (tkEnemy.stuckFrames > 12) {
        tkEnemy.targetX = random(20, 110);
        tkEnemy.targetY = random(18, 56);
        tkEnemy.stuckFrames = 0;
      }
    } else {
      tkEnemy.stuckFrames = 0;
    }
    tkEnemy.lastX = tkEnemy.x;
    tkEnemy.lastY = tkEnemy.y;

    float edx = (seePlayer ? playerX : tkEnemy.targetX) - tkEnemy.x;
    float edy = (seePlayer ? playerY : tkEnemy.targetY) - tkEnemy.y;
    float eDist = sqrt(edx * edx + edy * edy);

    if (eDist < 4.0f && !seePlayer) {
      tkEnemy.targetX = random(20, 110);
      tkEnemy.targetY = random(18, 56);
    } else if (eDist > 1.0f) {
      float enemySpeed = (seePlayer ? 0.85f : 0.55f) * dt;
      float eNextX = tkEnemy.x + (edx / eDist) * enemySpeed;
      float eNextY = tkEnemy.y + (edy / eDist) * enemySpeed;

      for (int w = 0; w < currentTotalWalls; w++) {
        if (activeMaze[w].isVertical) {
          if (eNextY >= activeMaze[w].y1 - 4 &&
              eNextY <= activeMaze[w].y2 + 4) {
            if (abs(eNextX - activeMaze[w].x1) < 5)
              eNextX = tkEnemy.x;
          }
        } else {
          if (eNextX >= activeMaze[w].x1 - 4 &&
              eNextX <= activeMaze[w].x2 + 4) {
            if (abs(eNextY - activeMaze[w].y1) < 5)
              eNextY = tkEnemy.y;
          }
        }
      }
      tkEnemy.x = eNextX;
      tkEnemy.y = eNextY;
    }

    // AI 战术火控系统
    if (seePlayer) {
      tkEnemy.turretAngle = atan2(playerY - tkEnemy.y, playerX - tkEnemy.x);
      if (now - tkEnemy.lastShot > 800) {
        tkEnemy.lastShot = now;
        for (int b = 0; b < MAX_TRBL_BULLETS; b++) {
          if (!tkBullets[b].active) {
            tkBullets[b].active = true;
            tkBullets[b].bouncesLeft = 5;
            tkBullets[b].fromEnemy = true; // 👈 标记：AI 的炮弹
            tkBullets[b].x = tkEnemy.x + cos(tkEnemy.turretAngle) * 5.0f;
            tkBullets[b].y = tkEnemy.y + sin(tkEnemy.turretAngle) * 5.0f;
            tkBullets[b].vx = cos(tkEnemy.turretAngle) * 2.5f;
            tkBullets[b].vy = sin(tkEnemy.turretAngle) * 2.5f;
            break;
          }
        }
      }
    } else {
      tkEnemy.turretAngle += 0.08f * tkEnemy.sweepDir * dt;
      float centerAngle = atan2(playerY - tkEnemy.y, playerX - tkEnemy.x);
      if (abs(tkEnemy.turretAngle - centerAngle) > 0.6f) {
        tkEnemy.sweepDir = -tkEnemy.sweepDir;
      }

      if (now - tkEnemy.lastShot > 2300) {
        tkEnemy.lastShot = now;
        for (int b = 0; b < MAX_TRBL_BULLETS; b++) {
          if (!tkBullets[b].active) {
            tkBullets[b].active = true;
            tkBullets[b].bouncesLeft = 5;
            tkBullets[b].fromEnemy = true;
            tkBullets[b].x = tkEnemy.x + cos(tkEnemy.turretAngle) * 5.0f;
            tkBullets[b].y = tkEnemy.y + sin(tkEnemy.turretAngle) * 5.0f;
            tkBullets[b].vx = cos(tkEnemy.turretAngle) * 2.0f;
            tkBullets[b].vy = sin(tkEnemy.turretAngle) * 2.0f;
            break;
          }
        }
      }
    }
  }

  // === 6. UI 图形层全量渲染 ===
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("TANK TROUBLE"));
  display.setCursor(92, 0);
  display.print(F("WIN:"));
  display.print(tkScore);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  for (int w = 0; w < currentTotalWalls; w++) {
    display.drawLine(activeMaze[w].x1, activeMaze[w].y1, activeMaze[w].x2,
                     activeMaze[w].y2, SSD1306_WHITE);
  }

  // 绘制玩家
  int px = (int)playerX;
  int py = (int)playerY;
  display.drawRect(px - 3, py - 3, 7, 7, SSD1306_WHITE);
  display.drawPixel(px, py, SSD1306_WHITE);
  int gunX = px + (int)(cos(aimAngle) * 6.0f);
  int gunY = py + (int)(sin(aimAngle) * 6.0f);
  display.drawLine(px, py, gunX, gunY, SSD1306_WHITE);

  // 绘制人机
  if (tkEnemy.active) {
    int ex = (int)tkEnemy.x;
    int ey = (int)tkEnemy.y;
    display.drawRect(ex - 3, ey - 3, 7, 7, SSD1306_WHITE);
    display.fillRect(ex - 1, ey - 1, 3, 3, SSD1306_BLACK);
    int eGunX = ex + (int)(cos(tkEnemy.turretAngle) * 6.0f);
    int eGunY = ey + (int)(sin(tkEnemy.turretAngle) * 6.0f);
    display.drawLine(ex, ey, eGunX, eGunY, SSD1306_WHITE);
  }

  for (int i = 0; i < MAX_TRBL_BULLETS; i++) {
    if (tkBullets[i].active) {
      display.drawPixel((int)tkBullets[i].x, (int)tkBullets[i].y,
                        SSD1306_WHITE);
    }
  }
}
