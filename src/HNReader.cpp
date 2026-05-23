#include "HNReader.h"
#include "Config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_HN_ITEMS 10
static char hnTitles[MAX_HN_ITEMS][64]; // 每条标题裁剪存储 64 字节
static int hnItemCount = 0;
static int hnSelectIdx = 0;
static int hnScrollTop = 0;
static bool hnFetchLoaded = false;
static bool hnFetchError = false;

// 极其底层的非阻塞流式字符串查找过滤函数（省内存神器）
static bool fetchHackerNewsRSS() {
  WiFiClientSecure client;
  client.setInsecure(); // 绕过 ESP32 繁琐的证书链验证
  HTTPClient http;

  if (!http.begin(client, "https://news.ycombinator.com/rss"))
    return false;

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // 拿到响应数据流，避免全量 getString() 直接撑爆堆内存
  WiFiClient *stream = http.getStreamPtr();
  hnItemCount = 0;

  // 简易流式标签匹配状态机
  while (stream->connected() && hnItemCount < MAX_HN_ITEMS) {
    if (stream->find("<item>")) { // 定位到单条新闻载体
      if (stream->find("<title>")) {
        String title = stream->readStringUntil('</title>');

        // 过滤转义常见 XML 网页符号
        title.replace("&amp;", "&");
        title.replace("&#x27;", "'");
        title.replace("&quot;", "\"");
        title.replace("&lt;", "<");
        title.replace("&gt;", ">");

        strncpy(hnTitles[hnItemCount], title.c_str(), 63);
        hnTitles[hnItemCount][63] = '\0';
        hnItemCount++;
      }
    } else {
      break; // 找不到了，流结束
    }
  }
  http.end();
  return (hnItemCount > 0);
}

void handleHNReader(int vry, int vrx, bool clicked) {
  // 1. 未连接 WiFi 的防御空跑拦截
  if (WiFi.status() != WL_CONNECTED) {
    if (clicked)
      currentState = STATE_MAIN_MENU;
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print(F("WiFi Disconnected!"));
    display.setCursor(10, 40);
    display.print(F("[Click] to Return"));
    return;
  }

  // 2. 状态懒加载器触发
  if (!hnFetchLoaded) {
    display.clearDisplay();
    display.setCursor(15, 25);
    display.print(F("Fetching HN RSS..."));
    display.display();

    if (fetchHackerNewsRSS()) {
      hnFetchLoaded = true;
      hnFetchError = false;
    } else {
      hnFetchLoaded = true;
      hnFetchError = true;
    }
    hnSelectIdx = 0;
    hnScrollTop = 0;
    return;
  }

  if (hnFetchError) {
    if (clicked) {
      hnFetchLoaded = false;
    } // 点击重试
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print(F("Fetch RSS Failed!"));
    display.setCursor(10, 40);
    display.print(F("[Click] to Retry"));
    return;
  }

  // 3. 摇杆控制环路
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) { // 向上推
      hnSelectIdx = (hnSelectIdx == 0) ? hnItemCount - 1 : hnSelectIdx - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) { // 向下推
      hnSelectIdx = (hnSelectIdx + 1) % hnItemCount;
      lastJoyAction = millis();
    } else if (vrx < 1000) { // 向左推退出
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    }

    // 视口平滑滚动算法
    if (hnSelectIdx < hnScrollTop)
      hnScrollTop = hnSelectIdx;
    else if (hnSelectIdx >= hnScrollTop + 3)
      hnScrollTop = hnSelectIdx - 3 + 1;
  }

  // 4. 精确的多行排版渲染
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("HACKER NEWS"));
  display.setCursor(100, 0);
  display.print(hnSelectIdx + 1);
  display.print(F("/"));
  display.print(hnItemCount);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 每条 Item 高度占用 18 像素（含行高），提供完美的两行折行排版空间
  for (int i = 0; i < 3; i++) {
    int itemIdx = hnScrollTop + i;
    if (itemIdx >= hnItemCount)
      break;

    int yPos = 12 + (i * 17);
    bool isSelected = (itemIdx == hnSelectIdx);

    if (isSelected) {
      display.fillRect(0, yPos - 1, 128, 16, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    // 手写的高效两行自适应截断安全打印
    display.setCursor(2, yPos);
    char *titlePtr = hnTitles[itemIdx];
    int charLen = strlen(titlePtr);

    // 第一行直接打印前 20 个字符
    for (int j = 0; j < 20 && j < charLen; j++) {
      display.print(titlePtr[j]);
    }
    // 如果还没完，移到第二行继续打接下来的字符
    if (charLen > 20) {
      display.setCursor(2, yPos + 8);
      for (int j = 20; j < 40 && j < charLen; j++) {
        display.print(titlePtr[j]);
      }
      if (charLen > 40)
        display.print(F("..")); // 溢出小尾巴提示
    }
  }
  display.setTextColor(SSD1306_WHITE);
}
