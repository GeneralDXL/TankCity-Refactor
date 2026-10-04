#ifndef REFLECT_H
#define REFLECT_H

#include <QPointF>
#include <QRect>

/**
 * 网格世界的反弹几何（M4 步 2）。
 *
 * ## 为什么可以这么简单
 * 世界是**网格**的，所有物块都是**轴对齐矩形** ⇒ 接触面只有水平与竖直两种
 * ⇒ **法线只可能是四个方向之一**，不需要任意法线。
 *
 * 于是**不必给 `engine/physics` 加"接触信息/法线"输出** ✓ —— 判定方（`Map`）
 * 本来就同时握着子弹的命中盒与物块矩形，够用了。这条判断来自
 * `docs/plans/M4-玩法创新落地草案.md` 决议 D3（2026-10-03 定案）。
 *
 * ## 本文件只做几何
 * 不含任何"什么时候该反弹"的规则（那是 `bounceOn` / `maxBounces` 的事，属配置与 `Map`）。
 * 纯函数、无状态、可单测 —— 反弹最容易出错的抖动/极限环都源于"每帧重猜"，
 * 把判定做成无副作用的纯函数是防它的第一道闸。
 */
namespace tankcity::world {

/// 撞在物块的哪一面 —— 也就是法线指向（法线**背离物块、指向来弹**）。
enum class Face {
    Left,   ///< 撞在物块左面（来弹在物块左侧）⇒ 法线 (−1, 0)
    Right,  ///< 撞在物块右面 ⇒ 法线 (+1, 0)
    Top,    ///< 撞在物块上面 ⇒ 法线 (0, −1)
    Bottom  ///< 撞在物块下面 ⇒ 法线 (0, +1)
};

/// @p face 对应的单位法线（背离物块、指向来弹）。
QPointF normalOf(Face face);

/**
 * 判定 @p hitBox 撞在 @p wallRect 的哪一面。
 *
 * 判据是**最小穿透轴**：两轴各算一次重叠深度，小的那个轴就是接触面所在的轴。
 * 例：子弹从左边撞上一面竖墙，x 方向只重叠几像素、y 方向可能重叠几十像素
 * ⇒ 取 x 轴 ⇒ 撞在左/右面 ✓。
 *
 * **两轴深度相等**（正对角落命中）时固定取 **x 轴** —— 必须是确定性的：
 * 否则同一发子弹会在两个候选面之间来回跳（M3 的"贴角抽搐"正是这类
 * "无状态启发式每帧重猜"造成的，代价已经吃过一次）。
 */
Face faceOf(const QRect &hitBox, const QRect &wallRect);

/// 按 @p face 的法线反射速度向量：`v' = v − 2(v·n)n`。
QPointF reflectVector(const QPointF &velocity, Face face);

} // namespace tankcity::world

#endif // REFLECT_H
