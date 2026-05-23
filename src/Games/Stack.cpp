#include "../Config.h"
#include "../Games.h"

#define MAX_STACK_LAYERS 40
#define STACK_BLOCK_HEIGHT 5

static float stackBlockX = 0.0f;
static int stackBlockWidth = 45;
static float stackBlockSpeedX = 1.4f;
static bool stackIsFalling = false;
static float stackBlockY = 12.0f;
static int towerLayerCount = 0;
static int scoreStack = 0;
static bool gameOverStack = false;
static unsigned long lastStackTick = 0;
static int towerWidths[MAX_STACK_LAYERS];
static int towerXs[MAX_STACK_LAYERS];
static int cameraViewOffsetY = 0;
static bool joyMoveLatched = false;

void initStackGame() {
  scoreStack = 0;
  towerLayerCount = 0;
  gameOverStack = false;
  stackBlockWidth = 45;
  stackBlockX = (SCREEN_WIDTH - stackBlockWidth) / 2;
  stackBlockY = 12.0f;
  stackBlockSpeedX = 1.4f;
  stackIsFalling = false;
  cameraViewOffsetY = 0;
  joyMoveLatched = false;
  memset(towerWidths, 0, sizeof(towerWidths));
  memset(towerXs, 0, sizeof(towerXs));
  towerWidths[0] = 50;
  towerXs[0] = (SCREEN_WIDTH - towerWidths[0]) / 2;
  towerLayerCount = 1;
  lastStackTick = millis();
}

void handleStackMode(int vry, int vrx, bool clicked) {
  if (gameOverStack) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(12, 35);
    display.print(F("Layers: "));
    display.println(towerLayerCount - 1);
    display.setCursor(12, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }
  bool joyMoved = (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000);
  if (joyMoved) {
    if (!joyMoveLatched && !stackIsFalling) {
      stackIsFalling = true;
      joyMoveLatched = true;
    }
  } else
    joyMoveLatched = false;

  if (millis() - lastStackTick > 25) {
    lastStackTick = millis();
    if (!stackIsFalling) {
      stackBlockX += stackBlockSpeedX;
      if (stackBlockX <= 0 || stackBlockX + stackBlockWidth >= SCREEN_WIDTH)
        stackBlockSpeedX = -stackBlockSpeedX;
    } else {
      stackBlockY += 3.5f;
      int targetTopY =
          58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
      if (stackBlockY >= targetTopY) {
        stackBlockY = targetTopY;
        stackIsFalling = false;
        int baseLeft = towerXs[towerLayerCount - 1];
        int baseRight = baseLeft + towerWidths[towerLayerCount - 1];
        int curLeft = (int)stackBlockX;
        int curRight = curLeft + stackBlockWidth;
        int overlapLeft = max(baseLeft, curLeft);
        int overlapRight = min(baseRight, curRight);
        if (overlapRight > overlapLeft) {
          towerXs[towerLayerCount] = overlapLeft;
          towerWidths[towerLayerCount] = overlapRight - overlapLeft;
          stackBlockWidth = towerWidths[towerLayerCount];
          scoreStack += 10;
          towerLayerCount++;
          if (towerLayerCount >= MAX_STACK_LAYERS) {
            initStackGame();
            return;
          }
          int currentHighestY =
              58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
          if (currentHighestY < 24)
            cameraViewOffsetY += STACK_BLOCK_HEIGHT;
          stackBlockX = random(0, SCREEN_WIDTH - stackBlockWidth);
          stackBlockY = 12.0f;
          stackBlockSpeedX = (stackBlockSpeedX > 0 ? 1.0f : -1.0f) *
                             (1.2f + (towerLayerCount * 0.12f));
          if (abs(stackBlockSpeedX) > 4.5f)
            stackBlockSpeedX = (stackBlockSpeedX > 0 ? 4.5f : -4.5f);
        } else
          gameOverStack = true;
      }
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("STACK: "));
  display.print(towerLayerCount - 1);
  display.setCursor(80, 0);
  display.print(F("S:"));
  display.print(scoreStack);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
  for (int i = 0; i < towerLayerCount; i++) {
    int drawY = 58 - ((i + 1) * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
    if (drawY > 64 || drawY < 10)
      continue;
    display.fillRect(towerXs[i], drawY, towerWidths[i], STACK_BLOCK_HEIGHT - 1,
                     SSD1306_WHITE);
  }
  display.drawRect((int)stackBlockX, (int)stackBlockY, stackBlockWidth,
                   STACK_BLOCK_HEIGHT - 1, SSD1306_WHITE);
}
