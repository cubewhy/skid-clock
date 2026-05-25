#include "../Config.h"
#include "../Games.h"

static const char *gameMenuItems[] = {
    "1. Snake Game",   "2. Gomoku Game",   "3. 2048 Game",
    "4. Dino Run",     "5. Brick Breaker", "6. Stack Tower",
    "7. Naval Battle", "8. Gold Miner",    "9. < Back"};
static const int GAMES_TOTAL = 9;
static const int VISIBLE_GAMES_ITEMS = 4;
static int currentGamesSelect = 0;
static int gamesScrollTop = 0;

void handleGamesMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentGamesSelect =
          (currentGamesSelect == 0) ? GAMES_TOTAL - 1 : currentGamesSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentGamesSelect = (currentGamesSelect + 1) % GAMES_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }

    if (currentGamesSelect < gamesScrollTop)
      gamesScrollTop = currentGamesSelect;
    else if (currentGamesSelect >= gamesScrollTop + VISIBLE_GAMES_ITEMS)
      gamesScrollTop = currentGamesSelect - VISIBLE_GAMES_ITEMS + 1;
  }

  if (selectTriggered) {
    switch (currentGamesSelect) {
    case 0:
      initSnakeGame();
      currentState = STATE_SNAKE;
      break;
    case 1:
      gomokuMenuSelect = 0;
      currentState = STATE_GOMOKU_MENU;
      break;
    case 2:
      init2048Game();
      currentState = STATE_2048;
      break;
    case 3:
      initDinoGame();
      currentState = STATE_DINO;
      break;
    case 4:
      initBrickGame();
      currentState = STATE_BRICK;
      break;
    case 5:
      initStackGame();
      currentState = STATE_STACK;
      break;
    case 6:
      initNavalGame();
      currentState = STATE_NAVAL_PLAY;
      break;
    case 7:
      initGoldMinerGame();
      currentState = STATE_GOLDMINER;
      break;
    case 8:
      currentState = STATE_MAIN_MENU;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== GAME CENTER ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  for (int i = 0; i < VISIBLE_GAMES_ITEMS; i++) {
    int itemIdx = gamesScrollTop + i;
    if (itemIdx >= GAMES_TOTAL)
      break;
    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (itemIdx == currentGamesSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(gameMenuItems[itemIdx]);
  }
  int barX = 124, barY = 14, barHeight = 34;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_GAMES_ITEMS / GAMES_TOTAL;
  int thumbY = barY + ((barHeight - thumbHeight) * currentGamesSelect /
                       (GAMES_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);
}
