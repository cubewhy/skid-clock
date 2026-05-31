#include "../Config.h"
#include "../DeveloperTools.h"
#include <IRrecv.h>
#include <IRutils.h>

// 声明定义在 main.cpp 中的全局红外接收实例
extern IRrecv irrecv;

// 定义一条红外捕获记录的轻量结构体，规避动态内存分配
struct IRRecord {
  decode_type_t
      protocolType; // 存储枚举类型，规避临时 String 销毁导致的悬空指针
  uint16_t address;
  uint16_t command;
  uint64_t rawData; // 升级为 uint64_t 以完整支持新库的 64 位红外码
};

#define MAX_IR_LOGS 3
static IRRecord irLogs[MAX_IR_LOGS];
static int logCount = 0;

void handleIRDebuggerMode(int vry, int vrx, bool clicked) {
  // 1. 摇杆向左拨动快捷返回主菜单
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    }
  }

  // 2. 物理中心键按下：清空当前捕获的历史记录槽
  if (clicked) {
    logCount = 0;
  }

  // 3. 非阻塞流式红外信号检测轮询
  decode_results results;
  if (irrecv.decode(&results)) {
    // 过滤掉红外协议中的长按连续重复帧 (repeat)，仅捕获有效单次按键
    if (!results.repeat) {

      // 将旧的记录在队列中向下串联位移
      for (int i = MAX_IR_LOGS - 1; i > 0; i--) {
        irLogs[i] = irLogs[i - 1];
      }

      // 压入最新的硬件解码数据
      irLogs[0].protocolType = results.decode_type;
      irLogs[0].address = results.address;
      irLogs[0].command = results.command;
      irLogs[0].rawData = results.value; // results.value 在新库中为 uint64_t

      if (logCount < MAX_IR_LOGS) {
        logCount++;
      }
    }
    // 释放锁存，使能下一次红外硬件 RMT 信号的接收
    irrecv.resume();
  }

  // 4. UI 视窗渲染与高精排版
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 头部状态条
  display.setCursor(2, 0);
  display.print(F("=== IR DEBUGGER ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  if (logCount == 0) {
    // 空轨等待提示
    display.setCursor(16, 28);
    display.print(F("Waiting for IR..."));
    display.setCursor(10, 52);
    display.print(F("[Click] to Clear"));
  } else {
    // A. 顶部黄金区域：展示当前最新捕获的详细报文内容
    display.setCursor(2, 14);
    display.print(F("NEW: "));
    // 使用新库配套的 typeToString 函数将协议枚举实时转换为字符串
    display.print(typeToString(irLogs[0].protocolType).c_str());

    char hexBuf[32];
    snprintf(hexBuf, sizeof(hexBuf), "A:0x%04X  C:0x%02X", irLogs[0].address,
             irLogs[0].command);
    display.setCursor(2, 24);
    display.print(hexBuf);

    // 针对 64 位数据进行安全的格式化输出
    snprintf(hexBuf, sizeof(hexBuf), "RAW: 0x%llX",
             (unsigned long long)irLogs[0].rawData);
    display.setCursor(2, 34);
    display.print(hexBuf);

    // 隔离虚线
    display.drawFastHLine(0, 44, 128, SSD1306_WHITE);

    // B. 底部紧凑区域：展示历史两段指令的滚动缓冲队列
    display.setCursor(2, 47);
    display.print(F("HIST1:"));
    if (logCount > 1) {
      snprintf(hexBuf, sizeof(hexBuf), "A:%04X C:%02X", irLogs[1].address,
               irLogs[1].command);
      display.print(hexBuf);
    } else {
      display.print(F(" --"));
    }

    display.setCursor(2, 56);
    display.print(F("HIST2:"));
    if (logCount > 2) {
      snprintf(hexBuf, sizeof(hexBuf), "A:%04X C:%02X", irLogs[2].address,
               irLogs[2].command);
      display.print(hexBuf);
    } else {
      display.print(F(" --"));
    }
  }
}
