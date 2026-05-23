#include "LobstersReader.h"
#include "Config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define MAX_NEWS_ITEMS 10
static char newsTitles[MAX_NEWS_ITEMS][64]; // 每条标题裁剪存储 64 字节
static int newsItemCount = 0;
static int newsSelectIdx = 0;
static int newsScrollTop = 0;
static bool newsFetchLoaded = false;
static bool newsFetchError = false;

// 极其底层的非阻塞流式字符串查找过滤函数
static bool fetchHackerNewsRSS() {
  WiFiClientSecure client;
  client.setInsecure(); // 绕过 ESP32 繁琐的证书链验证
  HTTPClient http;

  if (!http.begin(client, "https://lobste.rs/rss"))
    return false;

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // 拿到响应数据流，避免全量 getString() 直接撑爆堆内存
  WiFiClient *stream = http.getStreamPtr();
  newsItemCount = 0;

  // 简易流式标签匹配状态机
  while (stream->connected() && newsItemCount < MAX_NEWS_ITEMS) {
    if (stream->find("<item>")) { // 定位到单条新闻载体
      if (stream->find("<title>")) {
        String title = stream->readStringUntil('</title>');

        // 过滤转义常见 XML 网页符号
        title.replace("&amp;", "&");
        title.replace("&#x27;", "'");
        title.replace("&quot;", "\"");
        title.replace("&lt;", "<");
        title.replace("&gt;", ">");

        strncpy(newsTitles[newsItemCount], title.c_str(), 63);
        newsTitles[newsItemCount][63] = '\0';
        newsItemCount++;
      }
    } else {
      break; // 找不到了，流结束
    }
  }
  http.end();
  return (newsItemCount > 0);
}

void handleLobstersReader(int vry, int vrx, bool clicked) {
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
  if (!newsFetchLoaded) {
    display.clearDisplay();
    display.setCursor(15, 25);
    display.print(F("Fetching RSS..."));
    display.display();

    if (fetchHackerNewsRSS()) {
      newsFetchLoaded = true;
      newsFetchError = false;
    } else {
      newsFetchLoaded = true;
      newsFetchError = true;
    }
    newsSelectIdx = 0;
    newsScrollTop = 0;
    return;
  }

  if (newsFetchError) {
    if (clicked) {
      newsFetchLoaded = false;
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
      newsSelectIdx =
          (newsSelectIdx == 0) ? newsItemCount - 1 : newsSelectIdx - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) { // 向下推
      newsSelectIdx = (newsSelectIdx + 1) % newsItemCount;
      lastJoyAction = millis();
    } else if (vrx < 1000) { // 向左推退出
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    }

    // 视口平滑滚动算法
    if (newsSelectIdx < newsScrollTop)
      newsScrollTop = newsSelectIdx;
    else if (newsSelectIdx >= newsScrollTop + 3)
      newsScrollTop = newsSelectIdx - 3 + 1;
  }

  // 4. 精确的多行排版渲染
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("LOBSTERS"));
  display.setCursor(100, 0);
  display.print(newsSelectIdx + 1);
  display.print(F("/"));
  display.print(newsItemCount);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 每条 Item 高度占用 18 像素（含行高），提供完美的两行折行排版空间
  for (int i = 0; i < 3; i++) {
    int itemIdx = newsScrollTop + i;
    if (itemIdx >= newsItemCount)
      break;

    int yPos = 12 + (i * 17);
    bool isSelected = (itemIdx == newsSelectIdx);

    if (isSelected) {
      display.fillRect(0, yPos - 1, 128, 16, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    // 手写的高效两行自适应截断安全打印
    display.setCursor(2, yPos);
    char *titlePtr = newsTitles[itemIdx];
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
