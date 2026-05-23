#ifndef KEYBOARD_H
#define KEYBOARD_H

// 初始化通用键盘配置
// title: 顶部提示语，targetBuffer: 接收输入的缓冲区，maxLen:
// 限制长度，onConfirm: 点击OK后的回调函数
void initGlobalKeyboard(const char *title, char *targetBuffer, int maxLen,
                        void (*onConfirm)());

// 键盘控制流总线
void handleGlobalKeyboard(int vry, int vrx, bool clicked);

#endif
