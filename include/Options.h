#ifndef OPTIONS_H
#define OPTIONS_H

#if __has_include("OptOverrides.h")
#include "OptOverrides.h"
#define HAS_OVERRIDES 1
#else
#define HAS_OVERRIDES 0
#endif

// 在 OptOverrides.h 中定义相同的 macro 可以覆盖默认值。

#ifndef JJ_ENABLE_PROJ
// 跳一跳辅助线
#define JJ_ENABLE_PROJ 0
#endif // !JJ_ENABLE_PROJ

#ifndef GMT_OFFSET
// 时区 (用于 NTP 同步)
#define GMT_OFFSET 8
#endif // !GMT_OFFSET

// 可用 IR Debugger 获取按键码。
#ifndef IR_OVERRIDE
#define IR_REMOTE_CMD_UP 0x02
#define IR_REMOTE_CMD_DOWN 0x01
#define IR_REMOTE_CMD_LEFT 0x0C
#define IR_REMOTE_CMD_RIGHT 0x05
#define IR_REMOTE_CMD_OK 0x45
#define IR_REMOTE_CMD_BACK 0x49
#endif // !IR_OVERRIDE

#endif // OPTIONS_H
