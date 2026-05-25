#include "../Config.h"
#include "../Games.h"

enum MinerState { MINER_SWING, MINER_EXTEND, MINER_RETRACT };
static MinerState minerState = MINER_SWING;

static float hookAngle = 0.0f;
static float hookSpeed = 0.04f;
static float hookLength = 10.0f;
static const float launchSpeed = 2.2f;
static const float retractBaseSpeed = 2.0f;

struct GoldItem {
  int x;
  int y;
  uint8_t radius;
  int value;
  uint8_t weight;
  bool active;
};

#define TOTAL_MINERALS 8 // 提升至 8 个矿物，丰富地图密集度
static GoldItem minerals[TOTAL_MINERALS];
static int scoreMiner = 0;
static int grabbedIdx = -1;

static int minerLevel = 1;
static int targetScore = 0;
static unsigned long levelStartTime = 0;
static const unsigned long LEVEL_DURATION = 60000; // 每关 60 秒
static bool minerGameOver = false;
static bool minerLevelComplete = false;

// 矿物随机生成核心（带总价值动态回传）
static int generateMinerals() {
  int totalVal = 0;
  for (int i = 0; i < TOTAL_MINERALS; i++) {
    minerals[i].active = true;
    minerals[i].x = random(10, SCREEN_WIDTH - 10);
    minerals[i].y = random(28, SCREEN_HEIGHT - 8);

    int typeChance = random(0, 10); // 0-4 石头, 5-8 黄金, 9 钻石
    if (typeChance < 5) {
      // 【石头】
      minerals[i].radius = random(4, 7);
      minerals[i].value = random(15, 40);
      minerals[i].weight = random(4, 6);
    } else if (typeChance < 9) {
      // 【黄金】
      minerals[i].radius = random(3, 6);
      minerals[i].value = minerals[i].radius * 70;
      minerals[i].weight = random(2, 4);
    } else {
      // 【钻石】
      minerals[i].radius = 2;
      minerals[i].value = 400;
      minerals[i].weight = 1;
    }

    // 防重叠洗牌过滤
    for (int j = 0; j < i; j++) {
      float dist = sqrt(pow(minerals[i].x - minerals[j].x, 2) +
                        pow(minerals[i].y - minerals[j].y, 2));
      if (dist < (minerals[i].radius + minerals[j].radius + 4)) {
        minerals[i].x = random(10, SCREEN_WIDTH - 10);
        minerals[i].y = random(28, SCREEN_HEIGHT - 8);
        j = -1;
      }
    }
    // 实时累加这张图里的理论全量总金额
    totalVal += minerals[i].value;
  }
  return totalVal;
}

void initGoldMinerGame() {
  scoreMiner = 0;
  minerLevel = 1;
  minerGameOver = false;
  minerLevelComplete = false;
  minerState = MINER_SWING;
  hookAngle = 0.0f;
  hookLength = 10.0f;
  grabbedIdx = -1;
  levelStartTime = millis();

  // 第一关目标：必须拿下当前矿产总值的 50%
  int currentMapValue = generateMinerals();
  targetScore = currentMapValue * 0.50f;
}

void handleGoldMinerMode(int vry, int vrx, bool clicked) {
  // === 状态拦截器 1：游戏失败结算 ===
  if (minerGameOver) {
    if (clicked) {
      currentState = STATE_GAMES_MENU;
    }
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 34);
    display.print(F("Final Score: $"));
    display.println(scoreMiner);
    display.setCursor(10, 46);
    display.print(F("Target Was:  $"));
    display.println(targetScore);
    return;
  }

  // === 状态拦截器 2：成功过关过渡 ===
  if (minerLevelComplete) {
    if (clicked || vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000) {
      minerLevel++;

      // 生成下一关新地图并拿到新总价
      int currentMapValue = generateMinerals();

      // 动态梯度难度控制：每一关要求的吸金比例提高 2%，最高到 75% 封顶
      float targetRatio = 0.50f + (minerLevel * 0.02f);
      if (targetRatio > 0.75f)
        targetRatio = 0.75f;

      // 累计目标金额 = 上关要求基线 + (新地图总价值 * 难度系数)
      targetScore += (currentMapValue * targetRatio);

      minerState = MINER_SWING;
      hookAngle = 0.0f;
      hookLength = 10.0f;
      grabbedIdx = -1;
      minerLevelComplete = false;
      levelStartTime = millis();
    }
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(2);
    display.setCursor(10, 12);
    display.println(F("LEVEL PASS"));
    display.setTextSize(1);
    display.setCursor(10, 36);
    display.print(F("Current: $"));
    display.println(scoreMiner);
    display.setCursor(10, 48);
    display.println(F("[Action] Next Level"));
    return;
  }

  // === 核心游戏主循环 ===
  long timePassed = millis() - levelStartTime;
  long timeLeft = (LEVEL_DURATION - timePassed) / 1000;
  if (timeLeft < 0)
    timeLeft = 0;

  // 时限到期判定
  if (timeLeft <= 0) {
    if (scoreMiner >= targetScore) {
      minerLevelComplete = true;
    } else {
      minerGameOver = true;
    }
    return;
  }

  // 钩爪物理状态机
  if (minerState == MINER_SWING) {
    hookAngle += hookSpeed;
    if (hookAngle > 1.3f || hookAngle < -1.3f) {
      hookSpeed = -hookSpeed;
    }

    // 任意方向摇杆或按键均可触发弹射
    if (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000 || clicked) {
      minerState = MINER_EXTEND;
    }
  } else if (minerState == MINER_EXTEND) {
    hookLength += launchSpeed;
    float currX = 64 + sin(hookAngle) * hookLength;
    float currY = 14 + cos(hookAngle) * hookLength;

    if (currX < 0 || currX > SCREEN_WIDTH || currY > SCREEN_HEIGHT) {
      minerState = MINER_RETRACT;
      grabbedIdx = -1;
    }

    for (int i = 0; i < TOTAL_MINERALS; i++) {
      if (minerals[i].active) {
        float distance =
            sqrt(pow(currX - minerals[i].x, 2) + pow(currY - minerals[i].y, 2));
        if (distance < minerals[i].radius + 2) {
          minerState = MINER_RETRACT;
          grabbedIdx = i;
          break;
        }
      }
    }
  } else if (minerState == MINER_RETRACT) {
    float currRetractSpeed = retractBaseSpeed;
    if (grabbedIdx != -1) {
      currRetractSpeed /= minerals[grabbedIdx].weight;
      if (currRetractSpeed < 0.3f)
        currRetractSpeed = 0.3f;
    }

    hookLength -= currRetractSpeed;

    if (grabbedIdx != -1) {
      minerals[grabbedIdx].x = 64 + sin(hookAngle) * hookLength;
      minerals[grabbedIdx].y = 14 + cos(hookAngle) * hookLength;
    }

    if (hookLength <= 10.0f) {
      hookLength = 10.0f;
      minerState = MINER_SWING;
      if (grabbedIdx != -1) {
        scoreMiner += minerals[grabbedIdx].value;
        minerals[grabbedIdx].active = false;
        grabbedIdx = -1;

        // 地图提前清空判定
        bool anyMineralLeft = false;
        for (int i = 0; i < TOTAL_MINERALS; i++) {
          if (minerals[i].active) {
            anyMineralLeft = true;
            break;
          }
        }
        if (!anyMineralLeft) {
          if (scoreMiner >= targetScore) {
            minerLevelComplete = true;
          } else {
            minerGameOver = true;
          }
          return;
        }
      }
    }
  }

  // === 绘图渲染层 ===
  display.clearDisplay();
  display.setTextWrap(false); // 强制关闭折行拦截
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(2, 0);
  display.print(F("L"));
  display.print(minerLevel); // X=2 处开始：支持到 L99 (占 18 像素)

  display.setCursor(24, 0);
  display.print(F("G"));
  display.print(targetScore); // X=24 处开始：支持到 G12500 (占 36 像素)

  display.setCursor(68, 0);
  display.print(F("T"));
  display.print(timeLeft); // X=68 处开始：支持到 T60 (占 18 像素)

  display.setCursor(90, 0);
  display.print(F("$"));
  display.print(scoreMiner); // X=90 处开始：支持到 $15400 (占 36 像素，刚好到
                             // 126 像素完美收官)

  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  // 滑轮机台支架
  display.fillRect(60, 11, 9, 3, SSD1306_WHITE);

  // 绳索与钩子
  int endX = 64 + sin(hookAngle) * hookLength;
  int endY = 14 + cos(hookAngle) * hookLength;
  display.drawLine(64, 14, endX, endY, SSD1306_WHITE);
  display.drawCircle(endX, endY, 2, SSD1306_WHITE);

  // 渲染地底矿石
  for (int i = 0; i < TOTAL_MINERALS; i++) {
    if (minerals[i].active) {
      if (minerals[i].value >= 100) {
        display.drawCircle(minerals[i].x, minerals[i].y, minerals[i].radius,
                           SSD1306_WHITE);
        display.fillCircle(minerals[i].x, minerals[i].y, minerals[i].radius - 1,
                           SSD1306_WHITE);
      } else {
        display.drawRect(minerals[i].x - minerals[i].radius,
                         minerals[i].y - minerals[i].radius,
                         minerals[i].radius * 2, minerals[i].radius * 2,
                         SSD1306_WHITE);
      }
    }
  }
}
