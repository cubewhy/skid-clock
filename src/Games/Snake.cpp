#include "../Config.h"
#include "../Games.h"

#define SNAKE_BLOCK_SIZE 4
#define GRID_WIDTH (SCREEN_WIDTH / SNAKE_BLOCK_SIZE)
#define GRID_HEIGHT (SCREEN_HEIGHT / SNAKE_BLOCK_SIZE)
#define SNAKE_MAX_LEN 64

static Point snake[SNAKE_MAX_LEN];
static int snakeLength = 3;
static int score = 0;
static SnakeDirection snakeDir = SNAKE_RIGHT;
static SnakeDirection lastExecutedDir = SNAKE_RIGHT;
static Point food;
static uint8_t foodSize = 1;
static bool isGameOver = false;
static unsigned long lastSnakeUpdate = 0;
static const int snakeSpeed = 150;

static void spawnFood() {
  int attempts = 0;
  bool collision;

  do {
    collision = false;
    // 随机几率生成不同尺寸的食物
    int randVal = random(0, 10);
    if (randVal < 6) {
      foodSize = 1; // 60% 基础食物
    } else if (randVal < 9) {
      foodSize = 2; // 30% 中型食物
    } else {
      foodSize = 4; // 10% 巨型食物
    }

    food.x = random(0, GRID_WIDTH - foodSize + 1);
    food.y = random(0, GRID_HEIGHT - foodSize + 1);

    // 检查是否与蛇身重叠
    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x >= food.x && snake[i].x < food.x + foodSize &&
          snake[i].y >= food.y && snake[i].y < food.y + foodSize) {
        collision = true;
        break;
      }
    }

    attempts++;
    // 如果尝试了100次依然碰撞（通常是身体太长占满了空间），强制改为1x1食物减少碰撞率
    if (attempts > 100 && foodSize > 1) {
      foodSize = 1;
    }
    // 如果极端情况下依然无法找到完全空闲位置，摆脱循环，避免看门狗复位卡死
    if (attempts > 200) {
      break;
    }
  } while (collision);
}

void initSnakeGame() {
  snakeLength = 3;
  snake[0] = {15, 8};
  snake[1] = {14, 8};
  snake[2] = {13, 8};
  snakeDir = SNAKE_RIGHT;
  lastExecutedDir = SNAKE_RIGHT; // 初始化方向锁
  isGameOver = false;
  spawnFood();
}

void handleSnakeMode(int vry, int vrx, bool clicked) {
  if (isGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(score);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    display.display(); // 确保渲染物理屏幕
    return;
  }

  if (vrx < 1000 && lastExecutedDir != SNAKE_RIGHT)
    snakeDir = SNAKE_LEFT;
  else if (vrx > 3000 && lastExecutedDir != SNAKE_LEFT)
    snakeDir = SNAKE_RIGHT;
  else if (vry < 1000 && lastExecutedDir != SNAKE_DOWN)
    snakeDir = SNAKE_UP;
  else if (vry > 3000 && lastExecutedDir != SNAKE_UP)
    snakeDir = SNAKE_DOWN;

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  if (millis() - lastSnakeUpdate > snakeSpeed) {
    lastSnakeUpdate = millis();
    lastExecutedDir = snakeDir; // 在每次真正产生位移时，锁定本次方向

    // 记录移动前的尾部坐标，用于后续平滑生长增加节点
    Point oldTail = snake[snakeLength - 1];

    // 移动蛇身
    for (int i = snakeLength - 1; i > 0; i--)
      snake[i] = snake[i - 1];

    // 移动蛇头
    if (snakeDir == SNAKE_UP)
      snake[0].y--;
    else if (snakeDir == SNAKE_DOWN)
      snake[0].y++;
    else if (snakeDir == SNAKE_LEFT)
      snake[0].x--;
    else if (snakeDir == SNAKE_RIGHT)
      snake[0].x++;

    // 穿越墙壁逻辑
    if (snake[0].x < 0)
      snake[0].x = GRID_WIDTH - 1;
    else if (snake[0].x >= GRID_WIDTH)
      snake[0].x = 0;

    if (snake[0].y < 0)
      snake[0].y = GRID_HEIGHT - 1;
    else if (snake[0].y >= GRID_HEIGHT)
      snake[0].y = 0;

    // 咬到自身判定死亡
    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        isGameOver = true;
        return;
      }
    }

    // 精准碰撞感知
    if (snake[0].x >= food.x && snake[0].x < food.x + foodSize &&
        snake[0].y >= food.y && snake[0].y < food.y + foodSize) {

      int growFactor = foodSize * foodSize;
      for (int g = 0; g < growFactor; g++) {
        if (snakeLength < SNAKE_MAX_LEN) {
          snake[snakeLength] = oldTail;
          ++snakeLength;
        }
        ++score;
      }
      spawnFood();
    }
  }

  display.clearDisplay();
  display.setTextWrap(false);

  // 游戏内左上角实时显示玩家当前分数
  display.setCursor(2, 2);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(F("Score: "));
  display.print(score);

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

  // 大食物饱满填充
  if (foodSize > 1) {
    display.fillRect(food.x * SNAKE_BLOCK_SIZE + 1,
                     food.y * SNAKE_BLOCK_SIZE + 1, foodPixelSize - 2,
                     foodPixelSize - 2, SSD1306_WHITE);
  }

  display.display(); // 确保渲染物理屏幕
}
