/**
 * @file  reflect_test.cpp
 * @brief M4 步 2：反弹几何（法线四值 + 最小穿透轴 + 确定性优先级）。
 *
 * 这些是**纯函数**，所以断的不是"手感像不像反弹"，而是几何本身：
 * 法线方向、反射后的角度、以及最要紧的一条 —— **正对角落命中时必须是确定性的**
 * （两个轴穿透相等 ⇒ 固定取 x 轴）。M3 已经为一类"每帧重猜造成的抖动"
 * 付过账，这里在源头把它堵住。
 */

#include "reflect.h"

#include <QtMath>

#include <gtest/gtest.h>

using tankcity::world::Face;
using tankcity::world::faceOf;
using tankcity::world::normalOf;
using tankcity::world::reflectVector;

namespace {

constexpr double kEps = 1e-9;

/// 出射角度（度，0–360，与游戏里 bodyAngle/turretAngle 的取值域一致）。
double angleDegrees(const QPointF &v)
{
    double deg = qRadiansToDegrees(qAtan2(v.y(), v.x()));
    if (deg < 0)
        deg += 360.0;
    return deg;
}

} // namespace

/** 法线只有四个方向 —— 这是"网格世界"这一前提的直接结论。 */
TEST(Reflect, NormalsAreExactlyTheFourAxisDirections) {
    EXPECT_EQ(normalOf(Face::Left), QPointF(-1, 0));
    EXPECT_EQ(normalOf(Face::Right), QPointF(1, 0));
    EXPECT_EQ(normalOf(Face::Top), QPointF(0, -1));
    EXPECT_EQ(normalOf(Face::Bottom), QPointF(0, 1));
}

/** 子弹在物块左侧 → 撞的是**左面**，法线 −x；水平速度原样反向。 */
TEST(Reflect, BulletLeftOfAWallHitsTheLeftFace) {
    const QRect wall(100, 0, 40, 200);
    const QRect hit(95, 100, 10, 10);   // x 方向只重叠 5px，y 方向整格 → 取 x 轴

    ASSERT_EQ(faceOf(hit, wall), Face::Left);

    const QPointF out = reflectVector(QPointF(5, 0), Face::Left);
    EXPECT_NEAR(out.x(), -5, kEps);
    EXPECT_NEAR(out.y(), 0, kEps);
}

/** 子弹在物块上方 → 撞的是**上面**，法线 −y；竖直速度反向、水平不动。 */
TEST(Reflect, BulletAboveAWallHitsTheTopFace) {
    const QRect wall(0, 100, 200, 40);
    const QRect hit(100, 95, 10, 10);

    ASSERT_EQ(faceOf(hit, wall), Face::Top);

    const QPointF out = reflectVector(QPointF(0, -5), Face::Top);
    EXPECT_NEAR(out.x(), 0, kEps);
    EXPECT_NEAR(out.y(), 5, kEps);
}

/**
 * 斜射撞竖直面：只反转 x 分量，**y 分量保持**。
 * 这是"斜着打钢墙会沿着墙面滑走"的几何依据。
 */
TEST(Reflect, DiagonalOffAVerticalFaceFlipsOnlyX) {
    const QPointF out = reflectVector(QPointF(3, 4), Face::Left);
    EXPECT_NEAR(out.x(), -3, kEps);
    EXPECT_NEAR(out.y(), 4, kEps);
}

/**
 * 手稿里的例子：**45° 入射 → 出射 135°**。
 * 速度 (1,1) 撞竖墙 → (−1,1) → 角度 135°。
 */
TEST(Reflect, FourtyFiveDegreesComesOffAtOneThirtyFive) {
    const QPointF in(1, 1);
    EXPECT_NEAR(angleDegrees(in), 45.0, 1e-6);

    const QPointF out = reflectVector(in, Face::Left);
    EXPECT_NEAR(angleDegrees(out), 135.0, 1e-6);
}

/**
 * **正对角落命中**（两轴穿透相等）→ 固定取 x 轴。
 *
 * 几何完全对称时，若判据没有确定性优先级，同一发子弹会在"撞左面"与"撞上面"
 * 之间摇摆 —— 表现就是贴角抖动。这里把结论钉死：**答案是 x 轴的面**。
 */
TEST(Reflect, ExactCornerIsDeterministicAndPrefersTheXAxis) {
    const QRect wall(0, 0, 10, 10);
    const QRect hit(5, 5, 10, 10);   // overlapX = overlapY = 5

    const Face face = faceOf(hit, wall);
    EXPECT_NE(normalOf(face).x(), 0.0)
        << "两轴穿透相等时必须落在 x 轴上（确定性），而不是 y 轴";
    // 命中盒中心 (10,10) 在物块中心 (5,5) 右侧 → 取右面
    EXPECT_EQ(face, Face::Right);
}

/** 反射两次等于没反射（同一面）—— 顺带确认公式没有把速度缩放。 */
TEST(Reflect, ReflectingTwiceOnTheSameFaceRestoresTheVelocity) {
    const QPointF v(3.5, -1.25);
    const QPointF once = reflectVector(v, Face::Bottom);
    const QPointF twice = reflectVector(once, Face::Bottom);

    EXPECT_NEAR(twice.x(), v.x(), kEps);
    EXPECT_NEAR(twice.y(), v.y(), kEps);
}
