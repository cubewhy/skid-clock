#ifndef GAMES_H
#define GAMES_H

// 跨文件共享的特定游戏状态
extern int gomokuMenuSelect;

// 1. 街机中心二级菜单主入口
void handleGamesMenu(int vry, int vrx, bool clicked);

// 2. 各个子游戏的初始化与主循环入口
void initSnakeGame();
void handleSnakeMode(int vry, int vrx, bool clicked);

void initGomokuGame();
void handleGomokuMenu(int vry, int vrx, bool clicked);
void handleGomokuPlay(int vry, int vrx, bool clicked);

void init2048Game();
void handle2048Mode(int vry, int vrx, bool clicked);

void initDinoGame();
void handleDinoMode(int vry, int vrx, bool clicked);

void initBrickGame();
void handleBrickMode(bool clicked);

void initStackGame();
void handleStackMode(int vry, int vrx, bool clicked);

void initNavalGame();
void handleNavalPlay(int vry, int vrx, bool clicked);

void initGoldMinerGame();
void handleGoldMinerMode(int vry, int vrx, bool clicked);

void initFreeKeyGame();
void handleFreeKeyMode(int vry, int vrx, bool clicked);

void initPacmanGame();
void handlePacmanMode(int vry, int vrx, bool clicked);

void initShooterGame();
void handleShooterMode(int vry, int vrx, bool clicked);

void initTetrisGame();
void handleTetrisMode(int vry, int vrx, bool clicked);

void initTargetGame();
void handleTargetMode(int vry, int vrx, bool clicked);

void initRacing3D();
void handleRacing3DMode(int vry, int vrx, bool clicked);

void initRunner3D();
void handleRunner3DMode(int vry, int vrx, bool clicked);

void initTankTroubleGame();
void handleTankTroubleMode(int vry, int vrx, bool clicked);

void initUndertaleGame();
void handleUndertaleMode(int vry, int vrx, bool clicked);

void initJumpJumpGame();
void handleJumpJumpMode(int vry, int vrx, bool clicked);

void initFlappyGame();
void handleFlappyMode(int vry, int vrx, bool clicked);

void initPinGame();
void handlePinGameMode(int vry, int vrx, bool clicked);

void initWhacGame();
void handleWhacGameMode(int vry, int vrx, bool clicked);

#endif
