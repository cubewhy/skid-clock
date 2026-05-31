#include "../Config.h"
#include "../DeveloperTools.h"
#include <IRremote.hpp>

// 定义一条红外捕获记录的轻量结构体，规避动态内存分配
struct IRRecord {
  const char *protocolName;
  uint16_t address;
  uint16_t command;
  uint32_t rawData;
};

#define MAX_IR_LOGS 3
static IRRecord irLogs[MAX_IR_LOGS];
static int logCount = 0;

void initIR() { IrReceiver.begin(IR_RECV_PIN, DISABLE_LED_FEEDBACK); }

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
  if (IrReceiver.decode()) {
    // 过滤掉红外协议中的长按连续重复帧 (Repeat Flag)，仅捕获有效单次按键
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {

      // 将旧的记录在队列中向下串联位移
      for (int i = MAX_IR_LOGS - 1; i > 0; i--) {
        irLogs[i] = irLogs[i - 1];
      }

      // 压入最新的硬核解码数据
      irLogs[0].protocolName =
          getProtocolString(IrReceiver.decodedIRData.protocol);
      irLogs[0].address = IrReceiver.decodedIRData.address;
      irLogs[0].command = IrReceiver.decodedIRData.command;
      irLogs[0].rawData = IrReceiver.decodedIRData.decodedRawData;

      if (logCount < MAX_IR_LOGS) {
        logCount++;
      }
    }
    // 释放锁存，使能下一次红外脉冲中断信号的接收
    IrReceiver.resume();
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
    display.print(irLogs[0].protocolName);

    char hexBuf[32];
    snprintf(hexBuf, sizeof(hexBuf), "A:0x%04X  C:0x%02X", irLogs[0].address,
             irLogs[0].command);
    display.setCursor(2, 24);
    display.print(hexBuf);

    snprintf(hexBuf, sizeof(hexBuf), "RAW: 0x%08X", irLogs[0].rawData);
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
