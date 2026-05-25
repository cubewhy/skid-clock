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
static uint8_t foodSize = 1; // 食物尺寸比例：1->1x1, 2->2x2, 4->4x4
static bool isGameOver = false;
static unsigned long lastSnakeUpdate = 0;
static const int snakeSpeed = 150;

static void spawnFood() {
  // 机制更新：随机几率生成大食物
  int randVal = random(0, 10);
  if (randVal < 6) {
    foodSize = 1; // 60% 基础食物
  } else if (randVal < 9) {
    foodSize = 2; // 30% 2x2中型大食物
  } else {
    foodSize = 4; // 10% 4x4巨型大食物
  }

  food.x = random(0, GRID_WIDTH - foodSize + 1);
  food.y = random(0, GRID_HEIGHT - foodSize + 1);

  // 保证大食物安全跨域不产生蛇身重叠
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x >= food.x && snake[i].x < food.x + foodSize &&
        snake[i].y >= food.y && snake[i].y < food.y + foodSize) {
      food.x = random(0, GRID_WIDTH - foodSize + 1);
      food.y = random(0, GRID_HEIGHT - foodSize + 1);
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
    display.setTextWrap(false); // 明确不进行文本自动绕回
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

    // 核心修改 1：碰到墙壁时直接从对侧穿过去浮现，不再直接死亡
    if (snake[0].x < 0)
      snake[0].x = GRID_WIDTH - 1;
    else if (snake[0].x >= GRID_WIDTH)
      snake[0].x = 0;

    if (snake[0].y < 0)
      snake[0].y = GRID_HEIGHT - 1;
    else if (snake[0].y >= GRID_HEIGHT)
      snake[0].y = 0;

    // 核心修改 2：只在咬到自身内部环节时判定死亡
    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        isGameOver = true;
        return;
      }
    }

    // 核心修改 3：大食物范围碰撞盒精准交叉感知
    if (snake[0].x >= food.x && snake[0].x < food.x + foodSize &&
        snake[0].y >= food.y && snake[0].y < food.y + foodSize) {

      // 依食物物理大小获得等倍数的得分和更长的生长度
      int growFactor = foodSize * foodSize;
      for (int g = 0; g < growFactor; g++) {
        if (snakeLength < SNAKE_MAX_LEN)
          snakeLength++;
      }
      spawnFood();
    }
  }

  display.clearDisplay();
  display.setTextWrap(false);

  // 核心修改 4：游戏内左上角实时显示玩家当前分数
  display.setCursor(2, 2);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(F("Score: "));
  display.print(snakeLength - 3);

  // 绘制蛇体
  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snake[i].x * SNAKE_BLOCK_SIZE,
                     snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1,
                     SNAKE_BLOCK_SIZE - 1, SSD1306_WHITE);
  }

  // 渲染食物
  int foodPixelSize = foodSize * SNAKE_BLOCK_SIZE;
  display.drawRect(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE,
                   foodPixelSize, foodPixelSize, SSD1306_WHITE);
  // 如果是大食物，内部进行饱满填充以区别视觉观感
  if (foodSize > 1) {
    display.fillRect(food.x * SNAKE_BLOCK_SIZE + 1,
                     food.y * SNAKE_BLOCK_SIZE + 1, foodPixelSize - 2,
                     foodPixelSize - 2, SSD1306_WHITE);
  }
}
