#include "../Config.h"
#include "../Games.h"

#define MAX_TARGETS 3
#define PLAY_ZONE_Y 10

struct GameTarget {
  float x, y;
  float maxRadius;
  float currentRadius;
  unsigned long spawnTime;
  uint32_t lifeTime;
  bool active;
};

static GameTarget tgList[MAX_TARGETS];

// 准星绝对坐标与平滑速度向量
static float crosshairX = 64.0f;
static float crosshairY = 37.0f;

static int tgtScore = 0;
static int tgtShotsFired = 0;
static int tgtHits = 0;
static bool tgtGameOver = false;

static unsigned long tgtStartTime = 0;
static const unsigned long TGT_DURATION = 45000; // 每局限时 45 秒
static unsigned long lastTgtTick = 0;

// 特效反馈：记录上一次命中的绝对坐标与涟漪扩散动画帧
static int lastHitX = -1;
static int lastHitY = -1;
static int8_t hitPulseRadius = -1;

// 在边界安全区内随机刷出一个新靶子
static void spawnSingleTarget(int idx) {
  tgList[idx].active = true;
  tgList[idx].maxRadius = random(6, 13); // 随机生成不同大小的靶子（越小越难打）
  tgList[idx].currentRadius = tgList[idx].maxRadius;

  // 预留边距，防止靶子画出屏幕外
  tgList[idx].x = random(tgList[idx].maxRadius + 2,
                         SCREEN_WIDTH - tgList[idx].maxRadius - 2);
  tgList[idx].y = random(PLAY_ZONE_Y + tgList[idx].maxRadius + 2,
                         SCREEN_HEIGHT - tgList[idx].maxRadius - 2);

  tgList[idx].spawnTime = millis();
  tgList[idx].lifeTime = random(2000, 4500); // 靶子存在时间：2 到 4.5 秒
}

void initTargetGame() {
  tgtScore = 0;
  tgtShotsFired = 0;
  tgtHits = 0;
  tgtGameOver = false;
  crosshairX = 64.0f;
  crosshairY = 37.0f;

  lastHitX = -1;
  lastHitY = -1;
  hitPulseRadius = -1;

  for (int i = 0; i < MAX_TARGETS; i++) {
    spawnSingleTarget(i);
    // 错开各个靶子的初始生存进度
    tgList[i].spawnTime -= random(0, 1000);
  }

  tgtStartTime = millis();
  lastTgtTick = millis();
}

void handleTargetMode(int vry, int vrx, bool clicked) {
  if (tgtGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("TIME'S UP"));
    display.setTextSize(1);
    display.setCursor(10, 32);
    display.print(F("Score: "));
    display.println(tgtScore);
    display.setCursor(10, 44);
    // 打印命中率数据统计
    display.print(F("Accuracy: "));
    display.print(tgtShotsFired > 0 ? (tgtHits * 100 / tgtShotsFired) : 0);
    display.println(F("%"));
    display.setCursor(10, 55);
    display.println(F("[Click] Return Menu"));
    return;
  }

  unsigned long now = millis();
  float dt = (float)(now - lastTgtTick) / 25.0f;
  lastTgtTick = now;
  if (dt > 3.0f)
    dt = 3.0f;

  // 1. ⏱️ 倒计时结算拦截
  long timeLeft = (TGT_DURATION - (now - tgtStartTime)) / 1000;
  if (timeLeft <= 0) {
    tgtGameOver = true;
    return;
  }

  // 2. 🎯 摇杆控制准星平滑全向向量机动
  float moveSpeed = 1.8f * dt;
  if (vrx < 1000)
    crosshairX -= moveSpeed;
  else if (vrx > 3000)
    crosshairX += moveSpeed;
  if (vry < 1000)
    crosshairY -= moveSpeed;
  else if (vry > 3000)
    crosshairY += moveSpeed;

  // 准星物理安全墙拦截
  crosshairX = constrain(crosshairX, 2, SCREEN_WIDTH - 3);
  crosshairY = constrain(crosshairY, PLAY_ZONE_Y + 2, SCREEN_HEIGHT - 3);

  // 3. ⏳ 靶子生命周期倒计时衰减与自动枯竭置换
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!tgList[i].active) {
      spawnSingleTarget(i);
      continue;
    }

    unsigned long elapsed = now - tgList[i].spawnTime;
    if (elapsed >= tgList[i].lifeTime) {
      // 靶子超时未击中，自动报废并扣减 10 分惩罚分
      tgtScore = constrain(tgtScore - 10, 0, 9999);
      spawnSingleTarget(i);
    } else {
      // 靶子随着时间流逝，体积会逐渐收缩，提高后期瞄准难度
      float lifeRatio = (float)elapsed / tgList[i].lifeTime;
      tgList[i].currentRadius = tgList[i].maxRadius * (1.0f - lifeRatio * 0.4f);
    }
  }

  // 4. 💥 扣动扳机射击判定
  if (clicked) {
    tgtShotsFired++;
    bool anyHit = false;
    float closestDist = 999.0f;
    int hitIdx = -1;

    // 扫描距离准星最近的合法靶子
    for (int i = 0; i < MAX_TARGETS; i++) {
      if (!tgList[i].active)
        continue;
      float dist = sqrt(pow(crosshairX - tgList[i].x, 2) +
                        pow(crosshairY - tgList[i].y, 2));
      if (dist <= tgList[i].currentRadius && dist < closestDist) {
        closestDist = dist;
        hitIdx = i;
        anyHit = true;
      }
    }

    if (anyHit && hitIdx != -1) {
      tgtHits++;
      // 经典靶场解析得分模型：基础分 50 + 完美度精准线性映射
      // 如果正中靶心（closestDist=0），拿满 100 分；如果擦边，拿最低 50 分
      float perfection = 1.0f - (closestDist / tgList[hitIdx].currentRadius);
      int pointsEarned = 50 + (int)(perfection * 50);
      tgtScore += pointsEarned;

      // 激活命中爆炸波纹特效
      lastHitX = (int)tgList[hitIdx].x;
      lastHitY = (int)tgList[hitIdx].y;
      hitPulseRadius = 0;

      // 击毁旧靶，腾出槽位
      tgList[hitIdx].active = false;
    }
  }

  // === 5. UI 视窗图形层渲染 ===
  display.clearDisplay();

  // 顶层仪表盘状态数据栏
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("SCORE:"));
  display.print(tgtScore);
  display.setCursor(68, 0);
  display.print(F("TIME:"));
  display.print(timeLeft);
  display.print(F("s"));
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  // A. 渲染所有存活的同心圆靶子
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!tgList[i].active)
      continue;
    int tx = (int)tgList[i].x;
    int ty = (int)tgList[i].y;
    int r = (int)tgList[i].currentRadius;

    display.drawCircle(tx, ty, r, SSD1306_WHITE); // 外靶外沿框
    if (r > 5) {
      display.drawCircle(tx, ty, r / 2, SSD1306_WHITE); // 内靶十环核心线框
    }
    display.drawPixel(tx, ty, SSD1306_WHITE); // 绝对靶心点
  }

  // B. 异步步进渲染命中瞬间的扩展涟漪特效（每帧步进 1 像素，至 10 像素湮灭）
  if (hitPulseRadius >= 0) {
    display.drawCircle(lastHitX, lastHitY, hitPulseRadius, SSD1306_WHITE);
    // 通过交错像素绘制简易加号，提示命中文字效果
    display.setCursor(lastHitX - 8, lastHitY - 12);
    display.print(F("HIT!"));

    if ((now % 2) == 0)
      hitPulseRadius += 2; // 控制波纹扩散速率
    if (hitPulseRadius > 10)
      hitPulseRadius = -1; // 湮灭消除
  }

  // C. 绘制玩家的 2D 狙击准星十字架
  int cx = (int)crosshairX;
  int cy = (int)crosshairY;
  display.drawCircle(cx, cy, 3, SSD1306_WHITE);         // 准星中央环
  display.drawFastHLine(cx - 6, cy, 13, SSD1306_WHITE); // 横向准星分划线
  display.drawFastVLine(cx, cy - 6, 13, SSD1306_WHITE); // 纵向准星分划线
}
