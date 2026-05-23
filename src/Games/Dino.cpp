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
            (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
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
        obstacles[i].type = random(0, 3);
        if (obstacles[i].type == 0)
          obstacles[i].y = 42;
        else if (obstacles[i].type == 1)
          obstacles[i].y = 38;
        else
          obstacles[i].y = 32;
        lastObstacleSpawn = now;
        break;
      }
    }
  }
  display.clearDisplay();
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  if (dinoIsDucking)
    display.fillRect(15, 46, 14, 6, SSD1306_WHITE);
  else
    display.fillRect(15, (int)dinoY, 10, 12, SSD1306_WHITE);
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int ow =
          (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
      int oh =
          (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);
      if (obstacles[i].type == 2)
        display.drawRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
      else
        display.fillRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
    }
  }
  display.setTextSize(1);
  display.setCursor(90, 2);
  display.print((int)scoreDino);
}
