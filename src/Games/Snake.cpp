#include "../Config.h"
#include "../Games.h"

#define SNAKE_BLOCK_SIZE 4
#define GRID_WIDTH (SCREEN_WIDTH / SNAKE_BLOCK_SIZE)
#define GRID_HEIGHT (SCREEN_HEIGHT / SNAKE_BLOCK_SIZE)
#define SNAKE_MAX_LEN 64

static Point snake[SNAKE_MAX_LEN];
static int snakeLength = 3;
static SnakeDirection snakeDir = SNAKE_RIGHT;
static Point food;
static bool isGameOver = false;
static unsigned long lastSnakeUpdate = 0;
static const int snakeSpeed = 150;

static void spawnFood() {
  food.x = random(0, GRID_WIDTH);
  food.y = random(0, GRID_HEIGHT);
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x == food.x && snake[i].y == food.y) {
      food.x = random(0, GRID_WIDTH);
      food.y = random(0, GRID_HEIGHT);
      i = -1;
    }
  }
}

void initSnakeGame() {
  snakeLength = 3;
  snake[0] = {15, 8};
  snake[1] = {14, 8};
  snake[2] = {13, 8};
  snakeDir = SNAKE_RIGHT;
  isGameOver = false;
  spawnFood();
}

void handleSnakeMode(int vry, int vrx, bool clicked) {
  if (isGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(snakeLength - 3);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  if (vrx < 1000 && snakeDir != SNAKE_RIGHT)
    snakeDir = SNAKE_LEFT;
  else if (vrx > 3000 && snakeDir != SNAKE_LEFT)
    snakeDir = SNAKE_RIGHT;
  else if (vry < 1000 && snakeDir != SNAKE_DOWN)
    snakeDir = SNAKE_UP;
  else if (vry > 3000 && snakeDir != SNAKE_UP)
    snakeDir = SNAKE_DOWN;
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  if (millis() - lastSnakeUpdate > snakeSpeed) {
    lastSnakeUpdate = millis();
    for (int i = snakeLength - 1; i > 0; i--)
      snake[i] = snake[i - 1];
    if (snakeDir == SNAKE_UP)
      snake[0].y--;
    else if (snakeDir == SNAKE_DOWN)
      snake[0].y++;
    else if (snakeDir == SNAKE_LEFT)
      snake[0].x--;
    else if (snakeDir == SNAKE_RIGHT)
      snake[0].x++;

    if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH || snake[0].y < 0 ||
        snake[0].y >= GRID_HEIGHT) {
      isGameOver = true;
      return;
    }
    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        isGameOver = true;
        return;
      }
    }
    if (snake[0].x == food.x && snake[0].y == food.y) {
      if (snakeLength < SNAKE_MAX_LEN)
        snakeLength++;
      spawnFood();
    }
  }
  display.clearDisplay();
  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snake[i].x * SNAKE_BLOCK_SIZE,
                     snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1,
                     SNAKE_BLOCK_SIZE - 1, SSD1306_WHITE);
  }
  display.drawRect(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE,
                   SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE, SSD1306_WHITE);
}
