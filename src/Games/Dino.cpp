// src/Games/Dino.cpp
#include "../Config.h"
#include "../Games.h"

static float dinoY = 40.0f;
static float dinoVelocityY = 0.0f;
static bool dinoIsJumping = false;
static bool dinoIsDucking = false;
static float scoreDino = 0.0f;
static bool gameOverDino = false;
static unsigned long lastDinoUpdate = 0;
static float dinoGameSpeed = 0.10f;

static const float DINO_GRAVITY = 0.00055f;
static const float DINO_JUMP_FORCE = -0.17f;

#define MAX_OBSTACLES 2
static Obstacle obstacles[MAX_OBSTACLES];
static unsigned long lastObstacleSpawn = 0;

void initDinoGame() {
  dinoY = 40.0f;
  dinoVelocityY = 0.0f;
  dinoIsJumping = false;
  dinoIsDucking = false;
  scoreDino = 0.0f;
  gameOverDino = false;
  dinoGameSpeed = 0.10f;
  lastDinoUpdate = millis();
  lastObstacleSpawn = 0;
  for (int i = 0; i < MAX_OBSTACLES; i++)
    obstacles[i].active = false;
}

void handleDinoMode(int vry, int vrx, bool clicked) {
  if (gameOverDino) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println((int)scoreDino);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastDinoUpdate);
  lastDinoUpdate = now;
  if (dt > 50.0f)
    dt = 50.0f;
  if (dt <= 0.0f)
    dt = 1.0f;

  if (vry > 3000) {
    if (!dinoIsJumping)
      dinoIsDucking = true;
    else
      dinoVelocityY += DINO_GRAVITY * 2.5f * dt;
  } else
    dinoIsDucking = false;

  if (vry < 1000 && !dinoIsJumping && !dinoIsDucking) {
    dinoVelocityY = DINO_JUMP_FORCE;
    dinoIsJumping = true;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  scoreDino += 0.015f * dt;
  dinoGameSpeed += 0.000002f * dt;
  if (dinoGameSpeed > 0.22f)
    dinoGameSpeed = 0.22f;

  if (dinoIsJumping) {
    dinoY += dinoVelocityY * dt;
    dinoVelocityY += DINO_GRAVITY * dt;
    if (dinoY >= 40.0f) {
      dinoY = 40.0f;
      dinoIsJumping = false;
      dinoVelocityY = 0.0f;
    }
  }

  bool obstacleActive = false;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= dinoGameSpeed * dt;
      if (obstacles[i].x < -12.0f)
        obstacles[i].active = false;
      else {
        obstacleActive = true;

        int dw = dinoIsDucking ? 14 : 10;
        int dh = dinoIsDucking ? 6 : 12;
        int dy = dinoIsDucking ? 46 : (int)dinoY;
        int dx = 15;

        int ow =
            (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 12);
        int oh =
            (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);

        if (dx < (int)obstacles[i].x + ow && dx + dw > (int)obstacles[i].x &&
            dy < obstacles[i].y + oh && dy + dh > obstacles[i].y) {
          gameOverDino = true;
          return;
        }
      }
    }
  }

  uint32_t spawnInterval = 1500 / (dinoGameSpeed / 0.10f);
  if (spawnInterval < 600)
    spawnInterval = 600;

  if (!obstacleActive && (now - lastObstacleSpawn > spawnInterval)) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) {
        obstacles[i].active = true;
        obstacles[i].x = 128.0f;

        obstacles[i].type = random(0, 4);
        if (obstacles[i].type == 0)
          obstacles[i].y = 42;
        else if (obstacles[i].type == 1)
          obstacles[i].y = 38;
        else if (obstacles[i].type == 2)
          obstacles[i].y = 44;
        else
          obstacles[i].y = 39;

        lastObstacleSpawn = now;
        break;
      }
    }
  }

  // === UI 视窗图形层 ===
  display.clearDisplay();
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE); // 地平线

  // 👈【核心改进点】重构自机小恐龙的真像素级图形渲染
  int px = 15;
  if (dinoIsDucking) {
    // 🦖 动作 1：匍匐下蹲滑行状态 (完美贴合 14x6 碰撞体积)
    int py = 46;
    display.fillRect(px, py + 2, 9, 4, SSD1306_WHITE); // 后半段躯干与尾部
    display.fillRect(px + 6, py, 8, 4,
                     SSD1306_WHITE); // 向前伸出的长脖子与大脑袋
    display.drawPixel(px + 10, py + 1, SSD1306_BLACK); // 呆滞的小黑眼睛

    // 匍匐变频交替奔跑小碎步
    if ((now / 70) % 2 == 0) {
      display.drawPixel(px + 3, py + 5, SSD1306_BLACK); // 抬起左脚
      display.drawPixel(px + 6, py + 5, SSD1306_WHITE);
    } else {
      display.drawPixel(px + 3, py + 5, SSD1306_WHITE);
      display.drawPixel(px + 6, py + 5, SSD1306_BLACK); // 抬起右脚
    }
  } else {
    // 🦖 动作 2：标准站立/跃空状态 (完美贴合 10x12 碰撞体积)
    int py = (int)dinoY;
    display.fillRect(px + 4, py, 6, 4, SSD1306_WHITE);     // 标志性方形大脑袋
    display.drawPixel(px + 6, py + 1, SSD1306_BLACK);      // 标志性小呆萌眼睛
    display.fillRect(px + 4, py + 4, 3, 3, SSD1306_WHITE); // 结实的颈部
    display.fillRect(px + 1, py + 6, 8, 4, SSD1306_WHITE); // 敦实的下半身躯干
    display.drawPixel(px, py + 6, SSD1306_WHITE);          // 微微上翘的恐龙尾巴

    // 针对跳跃和地表奔跑进行双重足部动画仿真
    if (dinoIsJumping) {
      // 跃空悬浮时，双脚向下绷直收拢
      display.drawFastVLine(px + 3, py + 10, 2, SSD1306_WHITE);
      display.drawFastVLine(px + 6, py + 10, 2, SSD1306_WHITE);
    } else {
      // 在地面快速奔跑时，双脚交替离地抬升
      if ((now / 90) % 2 == 0) {
        display.drawFastVLine(px + 3, py + 10, 2, SSD1306_WHITE); // 左脚踩地
        display.drawPixel(px + 6, py + 10, SSD1306_WHITE);        // 右脚抬起
      } else {
        display.drawPixel(px + 3, py + 10, SSD1306_WHITE);        // 左脚抬起
        display.drawFastVLine(px + 6, py + 10, 2, SSD1306_WHITE); // 右脚踩地
      }
    }
  }

  // 渲染优化后的艺术景观障碍
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int ox = (int)obstacles[i].x;
      int oy = obstacles[i].y;

      if (obstacles[i].type == 0) {
        display.fillRect(ox + 2, oy, 2, 10, SSD1306_WHITE);
        display.drawFastVLine(ox, oy + 3, 4, SSD1306_WHITE);
        display.drawPixel(ox + 1, oy + 3, SSD1306_WHITE);
        display.drawFastVLine(ox + 5, oy + 1, 4, SSD1306_WHITE);
        display.drawPixel(ox + 4, oy + 1, SSD1306_WHITE);
      } else if (obstacles[i].type == 1) {
        display.fillRect(ox + 3, oy, 2, 14, SSD1306_WHITE);
        display.drawFastVLine(ox + 1, oy + 4, 5, SSD1306_WHITE);
        display.drawFastHLine(ox + 1, oy + 4, 2, SSD1306_WHITE);
        display.drawFastVLine(ox + 6, oy + 2, 6, SSD1306_WHITE);
        display.drawFastHLine(ox + 5, oy + 2, 2, SSD1306_WHITE);
      } else {
        display.fillRect(ox + 3, oy + 2, 5, 2, SSD1306_WHITE);
        display.drawPixel(ox + 8, oy + 1, SSD1306_WHITE);
        display.drawFastHLine(ox + 8, oy + 2, 4, SSD1306_WHITE);
        display.drawPixel(ox + 2, oy + 3, SSD1306_WHITE);

        if ((now / 130) % 2 == 0) {
          display.drawLine(ox + 4, oy + 2, ox + 2, oy, SSD1306_WHITE);
          display.drawLine(ox + 5, oy + 2, ox + 7, oy, SSD1306_WHITE);
        } else {
          display.drawLine(ox + 4, oy + 3, ox + 2, oy + 5, SSD1306_WHITE);
          display.drawLine(ox + 5, oy + 3, ox + 7, oy + 5, SSD1306_WHITE);
        }
      }
    }
  }

  display.setTextSize(1);
  display.setCursor(95, 2);
  display.print((int)scoreDino);
}
