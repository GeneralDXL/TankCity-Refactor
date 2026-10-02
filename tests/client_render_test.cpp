/**
 * @file  client_render_test.cpp
 * @brief 客户端地图绘制的回归测试：用「记账用的假渲染器」把绘制调用钉死。
 *
 * 为什么这么做：客户端的绘制本来是只能靠**肉眼看截图**验证的，而 M1.1 的验收
 * 口径恰恰是「画面与基线逐张一致」。把 `MapPainter` 的绘制路径收敛到
 * `engine::render::IRenderer` 之后，绘制调用就成了可以被断言的**数据**：
 * 顺序、矩形、颜色、用的是贴图还是纯色，都能在单元测试里精确固定下来。
 *
 * ## 覆盖范围（有意为之的边界）
 * 只覆盖**类型 0（边界）**这一支：它走纯色 + 黑边，不碰 `QPixmap`。
 * 带贴图的类型需要 `QPixmap`，而 `QPixmap` 只能在有 `QGuiApplication` 的进程里
 * 构造 —— 本测试进程里跑的是 `QCoreApplication`（见 engine_core_test.cpp），
 * 两者不能共存，且无头 CI 还要额外配 QPA 平台插件。所以贴图那一支交给
 * **人工截图对比**（`docs/testing/m1.1-baseline/` 的三张图），这里不硬凑。
 *
 * 顺带钉住一条最容易踩的细节：旧代码画边界用的是「当前画刷 + 当前画笔」的
 * `drawRect`，而当时的画笔是 QPainter 默认黑笔 —— 视觉上是**灰底 + 黑边**。
 * 如果谁把它改成只填充不描边，这个用例会立刻失败。
 */

#include <gtest/gtest.h>

#include <QColor>
#include <QRect>
#include <QVector>

#include "render/IRenderer.h"
#include "render/TextureCache.h"
#include "render/painter.h"

namespace {

/// 假渲染器：只记账，不真的画。
class RecordingRenderer : public engine::render::IRenderer
{
public:
    struct Call
    {
        bool isTexture = false;
        QRect rect;
        QColor fill;
        QColor border;
        qreal borderWidth = 0.0;
    };

    void drawTexture(const QPixmap &texture, const QRect &target) override
    {
        Q_UNUSED(texture);
        Call call;
        call.isTexture = true;
        call.rect = target;
        calls.append(call);
    }

    void drawRect(const QRect &rect, const QColor &fill, const QColor &border,
                  qreal borderWidth = 1.0) override
    {
        Call call;
        call.rect = rect;
        call.fill = fill;
        call.border = border;
        call.borderWidth = borderWidth;
        calls.append(call);
    }

    QVector<Call> calls;
};

} // namespace

TEST(ClientMapDraw, BoundaryBlockIsFilledGreyWithBlackBorder)
{
    RecordingRenderer renderer;
    engine::render::TextureCache textures(QStringLiteral("."));   // 纯色分支不会真的取贴图
    MapPainter map;

    map.addWall(WallPainter(10, 20, 30, 40, QStringLiteral("boundary"), 7));   // 边界走纯色分支
    map.draw(renderer, textures);

    ASSERT_EQ(renderer.calls.size(), 1);
    EXPECT_FALSE(renderer.calls.at(0).isTexture);
    EXPECT_EQ(renderer.calls.at(0).rect, QRect(10, 20, 30, 40));
    EXPECT_EQ(renderer.calls.at(0).fill, QColor(150, 150, 150));
    EXPECT_EQ(renderer.calls.at(0).border, QColor(Qt::black))
        << "边界必须是「灰底 + 黑边」：旧代码用默认黑笔 drawRect，丢了边框就与基线不一致";
}

TEST(ClientMapDraw, DrawsInWallInsertionOrderAtEachWallRect)
{
    RecordingRenderer renderer;
    engine::render::TextureCache textures(QStringLiteral("."));
    MapPainter map;

    // 三面边界，矩形各不相同：绘制顺序必须与加入顺序一致（后画的盖住先画的）。
    map.addWall(WallPainter(0, 0, 10, 10, QStringLiteral("boundary"), 1));
    map.addWall(WallPainter(10, 0, 10, 10, QStringLiteral("boundary"), 2));
    map.addWall(WallPainter(20, 0, 10, 10, QStringLiteral("boundary"), 3));
    map.draw(renderer, textures);

    ASSERT_EQ(renderer.calls.size(), 3);
    EXPECT_EQ(renderer.calls.at(0).rect, QRect(0, 0, 10, 10));
    EXPECT_EQ(renderer.calls.at(1).rect, QRect(10, 0, 10, 10));
    EXPECT_EQ(renderer.calls.at(2).rect, QRect(20, 0, 10, 10));
}

TEST(ClientMapDraw, RemovingAWallDropsItsDrawCall)
{
    RecordingRenderer renderer;
    engine::render::TextureCache textures(QStringLiteral("."));
    MapPainter map;

    map.addWall(WallPainter(0, 0, 10, 10, QStringLiteral("boundary"), 1));
    map.addWall(WallPainter(50, 0, 10, 10, QStringLiteral("boundary"), 2));

    map.removeWall(1);   // 模拟服务端下发 delete_wall
    map.draw(renderer, textures);

    ASSERT_EQ(renderer.calls.size(), 1);
    EXPECT_EQ(renderer.calls.at(0).rect, QRect(50, 0, 10, 10));
}

TEST(ClientMapDraw, EmptyMapDrawsNothing)
{
    RecordingRenderer renderer;
    engine::render::TextureCache textures(QStringLiteral("."));
    MapPainter map;

    map.draw(renderer, textures);

    EXPECT_TRUE(renderer.calls.isEmpty());
}
