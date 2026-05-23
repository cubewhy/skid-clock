#include "WiFiModule.h"
#include "Config.h"
#include "Keyboard.h"
#include <WiFi.h>

// 局部业务缓存
static const char *wifiMenuItems[] = {"1. Scan Networks", "2. Add Hidden WiFi",
                                      "3. < Back"};
static int currentWiFiSelect = 0;

static char wifiSSID[33] = "";
static char wifiPassword[65] = "";
static bool isHiddenMode = false;

// 异步扫描控制缓存
static int scanResultCount = -1;
static int scanScrollTop = 0;
static int scanSelectIdx = 0;
static bool scanInProgress = false;

// 非阻塞连接计时器
static unsigned long connectStartTime = 0;

// ==========================================
// 🚀 键盘确定按钮绑定的私有回调触发函数组
// ==========================================
static void onPasswordInputConfirm() {
  // 密码输入完成，改变状态到连接中，并启动连接
  currentState = STATE_WIFI_CONNECTING;
  connectStartTime = millis();
  WiFi.disconnect(true); // 清理旧连接
  WiFi.begin(wifiSSID, wifiPassword);
}

static void onHiddenSSIDConfirm() {
  // 隐藏SSID输入完成，复用键盘让其继续输入密码
  wifiPassword[0] = '\0';
  initGlobalKeyboard("Enter Hidden Pass:", wifiPassword, 65,
                     onPasswordInputConfirm);
  currentState = STATE_WIFI_KEYBOARD;
}

// ==========================================
// 📡 状态机分支处理器
// ==========================================
void handleWiFiMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentWiFiSelect = (currentWiFiSelect == 0) ? 2 : currentWiFiSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentWiFiSelect = (currentWiFiSelect + 1) % 3;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }
  }

  if (selectTriggered) {
    if (currentWiFiSelect == 0) {
      scanResultCount = -1;
      scanScrollTop = 0;
      scanSelectIdx = 0;
      scanInProgress = false;
      isHiddenMode = false;
      currentState = STATE_WIFI_SCAN;
    } else if (currentWiFiSelect == 1) {
      isHiddenMode = true;
      wifiSSID[0] = '\0';
      // 开启复用键盘输入SSID
      initGlobalKeyboard("Enter Hidden SSID:", wifiSSID, 33,
                         onHiddenSSIDConfirm);
      currentState = STATE_WIFI_KEYBOARD;
    } else {
      currentState = STATE_MAIN_MENU;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== WIFI MANAGER ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) {
    display.setCursor(5, 16 + (i * 12));
    if (i == currentWiFiSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(wifiMenuItems[i]);
  }
}

void handleWiFiScan(int vry, int vrx, bool clicked) {
  // 1. 发现扫描未启动，抛入后台异步进行
  if (!scanInProgress && scanResultCount == -1) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true); // true = 开启非阻塞异步扫描
    scanInProgress = true;
  }

  // 2. 检查异步扫描结果进度
  if (scanInProgress) {
    int16_t status = WiFi.scanComplete();
    if (status >= 0) { // 扫描圆满结束
      scanResultCount = status;
      scanInProgress = false;
    } else { // 还在扫描，绘制等待动画
      display.clearDisplay();
      display.setCursor(10, 20);
      display.print(F("Scanning WiFi..."));
      display.fillRect(10, 36, (millis() / 10) % 100, 4, SSD1306_WHITE);
      return;
    }
  }

  // 如果扫完发现没有任何热点
  if (scanResultCount == 0) {
    if (clicked)
      currentState = STATE_WIFI_MENU;
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print(F("No Networks Found."));
    display.setCursor(10, 40);
    display.print(F("[Click] to return"));
    return;
  }

  // 3. 结果列表滚动选择逻辑
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      scanSelectIdx =
          (scanSelectIdx == 0) ? scanResultCount - 1 : scanSelectIdx - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      scanSelectIdx = (scanSelectIdx + 1) % scanResultCount;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_WIFI_MENU;
      lastJoyAction = millis();
      return;
    }

    if (scanSelectIdx < scanScrollTop)
      scanScrollTop = scanSelectIdx;
    else if (scanSelectIdx >= scanScrollTop + 3)
      scanScrollTop = scanSelectIdx - 3 + 1;
  }

  if (clicked) {
    // 锁定选中网络的 SSID，去输密码
    strncpy(wifiSSID, WiFi.SSID(scanSelectIdx).c_str(), 32);
    wifiPassword[0] = '\0';
    initGlobalKeyboard("Enter Password:", wifiPassword, 65,
                       onPasswordInputConfirm);
    currentState = STATE_WIFI_KEYBOARD;
    return;
  }

  // 4. 渲染网络列表
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Select Network ("));
  display.print(scanResultCount);
  display.println(F(")"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < 3; i++) {
    int curIdx = scanScrollTop + i;
    if (curIdx >= scanResultCount)
      break;
    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (curIdx == scanSelectIdx)
      display.print(F("> "));
    else
      display.print(F("  "));

    // 显示SSID及截断防止爆屏
    String name = WiFi.SSID(curIdx);
    if (name.length() > 12)
      name = name.substring(0, 11) + "~";
    display.print(name);

    // 在右侧边缘打印信号格强弱 dBm
    display.setCursor(102, yPos);
    display.print(WiFi.RSSI(curIdx));
  }
}

void handleWiFiConnectingLoop() {
  display.clearDisplay();
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    display.setCursor(10, 15);
    display.print(F("CONNECTED SUCCESS!"));
    display.setCursor(10, 30);
    display.print(F("IP:"));
    display.setCursor(10, 42);
    display.print(WiFi.localIP());

    static bool ntpExecuted = false;
    if (!ntpExecuted) {
      display.setCursor(10, 32);
      display.print(F("NTP Syncing..."));
      display.display();

      // 配置时区（以东八区 UTC+8 为默认，采用国内+全球通用授时池）
      configTime(8 * 3600, 0, "ntp.ntsc.ac.cn", "pool.ntp.org");

      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 5000)) { // 5秒授时超时
        RtcDateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                            timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                            timeinfo.tm_sec);
        Rtc.SetDateTime(ntpTime); // 完美的将精确时间打入 DS1302 硬件
        display.setCursor(10, 44);
        display.print(F("RTC Synced OK!"));
      } else {
        display.setCursor(10, 44);
        display.print(F("NTP Timeout!"));
      }
      ntpExecuted = true;
    }

    display.setCursor(10, 54);
    display.print(F("[Back] to exit"));
    // 连上之后，依靠用户主动按[返回键]退出该长效看板
  } else if (status == WL_CONNECT_FAILED ||
             (millis() - connectStartTime > 15000)) {
    // 15秒连不上判定超时失败
    display.setCursor(10, 20);
    display.print(F("CONNECTION FAILED"));
    display.setCursor(10, 40);
    display.print(F("[Back] to retry"));
  } else {
    // 正在持续握手分配IP，绘制连接加载条
    display.setCursor(10, 15);
    display.print(F("Connecting to:"));
    display.setCursor(10, 27);
    display.print(wifiSSID);
    display.drawRect(10, 44, 100, 6, SSD1306_WHITE);
    int progress = map((millis() - connectStartTime) % 3000, 0, 3000, 0, 96);
    display.fillRect(12, 46, progress, 2, SSD1306_WHITE);
  }
}
