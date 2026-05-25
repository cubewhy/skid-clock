#ifndef ENGINE3D_H
#define ENGINE3D_H

#include <Arduino.h>

#define P3D_HORIZON 26  // 地平线在屏幕上的垂直高度 (Y轴)
#define P3D_CENTER_X 64 // 屏幕水平中心轴位置 (X轴)
#define P3D_FOCO 55     // 相机焦距 (Focal Length)，控制视野广角大小

// 伪 3D 空间内的基础物体线框结构
struct P3DObject {
  float x;  // 3D 世界相对赛道中轴的左右漂移量 (左负右正)
  float z;  // 3D 世界相对整条赛道的 Z 轴深度位置
  float y;  // 3D 世界相对地面的高度 (0 为贴地)
  int w, h; // 2D 基础立绘高宽尺寸
  bool active;
};

// 👈 核心抽象算法：将 3D 世界空间坐标，通过相机视角转换，投影到 2D OLED
// 点阵图上
static inline bool project3D(float x3d, float y3d, float z3d, float camX,
                             float camY, float camZ, int &screenX, int &screenY,
                             int &projectedScale) {
  // 计算物体到相机的相对 Z 轴深度
  float relativeZ = z3d - camZ;
  if (relativeZ <= 1.0f)
    return false; // 物体在相机后方或过于贴近，不予渲染

  // 计算透视缩放比 (深度越深，尺寸乘积因子越小)
  float scale = (float)P3D_FOCO / relativeZ;
  projectedScale = (int)(scale * 100); // 放大 100 倍提供缩放精度

  // 坐标系变换与视窗投影
  screenX = P3D_CENTER_X + (int)((x3d - camX) * scale);
  screenY = P3D_HORIZON + (int)((camY - y3d) * scale);
  return true;
}

#endif
