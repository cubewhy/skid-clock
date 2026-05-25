#include "../Config.h"
#include "../Games.h"

#define MAX_MY_BULLETS 8
#define MAX_ENEMIES 5
#define PLAYER_W 9
#define PLAYER_H 8

struct PlaneBullet {
  float x, y;
  bool active;
};

struct PlaneEnemy {
  float x, y;
  float speed;
  uint8_t hp;
  uint8_t type; // 0: 普通轻型（1HP）, 1: 重型中型机（3HP，体积大）
  bool active;
};

static PlaneBullet myBullets[MAX_MY_BULLETS];
static PlaneEnemy enemies[MAX_ENEMIES];

static float playerX = 64.0f;
static const int playerY = 54; // 战机固定盘踞在屏幕底部

static int shooterScore = 0;
static int playerHp = 3;
static int bombCount = 1; // 初始赠送 1 枚核弹
static bool shooterGameOver = false;

static unsigned long lastFireTime = 0;
static unsigned long lastEnemySpawn = 0;
static unsigned long lastShooterTick = 0;

// 初始化战斗参数
void initShooterGame() {
  shooterScore = 0;
  playerHp = 3;
  bombCount = 1;
  shooterGameOver = false;
  playerX = 64.0f;

  for (int i = 0; i < MAX_MY_BULLETS; i++)
    myBullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++)
    enemies[i].active = false;

  lastFireTime = millis();
  lastEnemySpawn = millis();
  lastShooterTick = millis();
}

// 释放清除全屏敌机的BOMB大招
static void triggerNuclearBomb() {
  if (bombCount <= 0)
    return;
  bombCount--;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      shooterScore += (enemies[i].type == 1) ? 30 : 10;
      enemies[i].active = false;
    }
  }
  // 产生全屏闪烁视觉反馈
  display.fillScreen(SSD1306_WHITE);
  display.display();
  delay(50);
}

void handleShooterMode(int vry, int vrx, bool clicked) {
  // 游戏结束拦截器
  if (shooterGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 12);
    display.println(F("YOU FAIL"));
    display.setTextSize(1);
    display.setCursor(10, 36);
    display.print(F("Final Score: "));
    display.println(shooterScore);
    display.setCursor(10, 50);
    display.println(F("[Click] to return"));
    return;
  }

  // 1. 电位器控制左右平滑移动
  int rawPot = analogRead(POT_PIN);
  // 标准阻尼归一化：将 0~4095 映射到屏幕安全宽度内
  float normPot = 1.0f - ((float)rawPot / 4095.0f);
  playerX = normPot * (SCREEN_WIDTH - PLAYER_W);
  playerX = constrain(playerX, 0, SCREEN_WIDTH - PLAYER_W);

  if (vry > 3000 && (millis() - lastJoyAction > JOY_DELAY)) {
    triggerNuclearBomb();
    lastJoyAction = millis(); // 锁定动作帧，防止身上多枚核弹被一帧内瞬间清空
  }

  unsigned long now = millis();
  float dt = (float)(now - lastShooterTick) / 30.0f; // 归一化步长
  lastShooterTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 3. 战机高频自动开火机制（无需搓按键，体验流畅）
  uint32_t fireInterval = 300; // 每 300ms 喷射一发子弹
  if (now - lastFireTime > fireInterval) {
    lastFireTime = now;
    for (int i = 0; i < MAX_MY_BULLETS; i++) {
      if (!myBullets[i].active) {
        myBullets[i].active = true;
        myBullets[i].x = playerX + PLAYER_W / 2; // 子弹从战机正中央射出
        myBullets[i].y = playerY - 2;
        break;
      }
    }
  }

  // 4. 敌机动态梯度概率生成
  uint32_t spawnInterval =
      constrain(1200 - (shooterScore * 2), 400, 1500); // 随分数增加出怪变快
  if (now - lastEnemySpawn > spawnInterval) {
    lastEnemySpawn = now;
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (!enemies[i].active) {
        enemies[i].active = true;
        enemies[i].x = random(0, SCREEN_WIDTH - 12);
        enemies[i].y = -10;

        // 25% 概率生成高血量重型机
        if (random(0, 100) < 25 && shooterScore > 100) {
          enemies[i].type = 1;
          enemies[i].hp = 3;
          enemies[i].speed = 0.6f + (random(0, 5) * 0.1f);
        } else {
          enemies[i].type = 0;
          enemies[i].hp = 1;
          enemies[i].speed =
              1.1f + (random(0, 8) * 0.1f) + (shooterScore * 0.002f);
        }
        break;
      }
    }
  }

  // 5. 游戏物理逻辑演算循环
  // A. 子弹位移与边缘注销
  for (int i = 0; i < MAX_MY_BULLETS; i++) {
    if (myBullets[i].active) {
      myBullets[i].y -= 3.5f * dt;
      if (myBullets[i].y < 10)
        myBullets[i].active = false; // 出界物理消除
    }
  }

  // B. 敌机物理演进与碰撞体积穿透感知
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active)
      continue;

    enemies[i].y += enemies[i].speed * dt;
    if (enemies[i].y > SCREEN_HEIGHT) {
      enemies[i].active = false;
      playerHp--; // 漏掉敌机突防，扣减基地生命值
      if (playerHp <= 0)
        shooterGameOver = true;
      continue;
    }

    int ew = (enemies[i].type == 1) ? 12 : 7;
    int eh = (enemies[i].type == 1) ? 10 : 6;

    // 子弹与当前敌机的碰撞判定
    for (int b = 0; b < MAX_MY_BULLETS; b++) {
      if (myBullets[b].active) {
        if (myBullets[b].x >= enemies[i].x &&
            myBullets[b].x <= enemies[i].x + ew &&
            myBullets[b].y >= enemies[i].y &&
            myBullets[b].y <= enemies[i].y + eh) {

          myBullets[b].active = false; // 碎弹
          enemies[i].hp--;

          if (enemies[i].hp <= 0) {
            enemies[i].active = false;
            shooterScore += (enemies[i].type == 1) ? 30 : 10;
            // 爆分奖励大招
            if (shooterScore % 400 == 0)
              bombCount = constrain(bombCount + 1, 0, 3);
          }
          break;
        }
      }
    }

    // 敌机与自机（Player）撞击判定
    if (enemies[i].active) {
      if (playerX < enemies[i].x + ew && playerX + PLAYER_W > enemies[i].x &&
          playerY < enemies[i].y + eh && playerY + PLAYER_H > enemies[i].y) {
        enemies[i].active = false;
        playerHp--;
        if (playerHp <= 0)
          shooterGameOver = true;
      }
    }
  }

  // === 6. UI 视窗渲染图形层 ===
  display.clearDisplay();

  // 顶层仪表盘数据区
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("SCORE:"));
  display.print(shooterScore);
  display.setCursor(72, 0);
  display.print(F("H:"));
  for (int h = 0; h < playerHp; h++)
    display.print(F("=")); // 用类似等号的血条块展示残机
  display.setCursor(110, 0);
  display.print(F("B:"));
  display.print(bombCount);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // 绘制玩家飞机（经典的三角大黄蜂机翼形态）
  int px = (int)playerX;
  display.drawTriangle(px + 4, playerY, px, playerY + 7, px + 8, playerY + 7,
                       SSD1306_WHITE);
  display.drawFastHLine(px + 2, playerY + 5, 5, SSD1306_WHITE);
  display.drawPixel(px + 4, playerY + 3, SSD1306_WHITE);

  // 绘制我方飞行子弹（双像素粒子光束）
  for (int i = 0; i < MAX_MY_BULLETS; i++) {
    if (myBullets[i].active) {
      display.drawFastVLine((int)myBullets[i].x, (int)myBullets[i].y, 2,
                            SSD1306_WHITE);
    }
  }

  // 绘制敌方突击方阵
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active)
      continue;
    int ex = (int)enemies[i].x;
    int ey = (int)enemies[i].y;

    if (enemies[i].type == 1) {
      // 重型轰炸机：大体积倒三角 + 侧翼实心填充（气势压迫）
      display.drawTriangle(ex + 6, ey + 9, ex, ey, ex + 12, ey, SSD1306_WHITE);
      display.fillRect(ex + 4, ey + 2, 5, 3, SSD1306_WHITE);
      display.drawFastHLine(ex + 2, ey, 9, SSD1306_WHITE);
    } else {
      // 普通轻型侦察机：简易倒小三角形态
      display.drawTriangle(ex + 3, ey + 5, ex, ey, ex + 6, ey, SSD1306_WHITE);
    }
  }
}
