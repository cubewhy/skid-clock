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
