#include "painter.h"
#include <QPainterPath>
#include<QDateTime>
#include <cmath>

WallPainter::WallPainter(int x, int y, int width, int height, int type, int id)
    : x(x), y(y), width(width), height(height), type(type), id(id)
{}

MapPainter::MapPainter()
{
    // 初始化地图数据
    mapIndex = 0; // 默认地图索引
}

namespace {

/// 物块类型 -> 贴图路径（相对资源根目录）。
///
/// **这张表就是 M6 换贴图时唯一要改的地方**。类型号的含义来自服务端协议里的
/// `wallType`（边界 0 / 砖 1 / 钢 2 / 森林 3 / 海 4 / 冰 5），客户端不 include
/// `game/entity/wall.h` —— 两个程序只通过 JSON 字段对齐（见 M1.1 决议 D4）。
struct BlockTextureEntry
{
    int type;
    const char *path;
};

const BlockTextureEntry kBlockTextures[] = {
    {1, "images/textures/brick.jpg"},
    {2, "images/textures/steel.jpg"},
    {3, "images/textures/forest.jpg"},
    {4, "images/textures/sea.jpg"},
    {5, "images/textures/ice.jpg"},
};

const char *texturePathOfType(int type)
{
    for (const BlockTextureEntry &entry : kBlockTextures) {
        if (entry.type == type)
            return entry.path;
    }
    return nullptr;
}

} // namespace

void MapPainter::draw(engine::render::IRenderer &renderer, engine::render::TextureCache &textures)
{
    for (const WallPainter &wall : walls)
    {
        const QRect rect = wall.getRect();
        const int type = wall.getType();

        if (type == 0) {
            // 旧代码这一支是 `setBrush(灰) + drawRect`，当时的画笔是 QPainter 的默认黑笔，
            // 所以实际画的是「灰底 + 黑边」。只填充不描边会丢掉那圈边框，故两个颜色都给。
            renderer.drawRect(rect, QColor(150, 150, 150), Qt::black);
            continue;
        }

        const char *path = texturePathOfType(type);
        if (path == nullptr)
            continue;   // 未知类型：旧 switch 没有 default，同样什么都不画

        renderer.drawTexture(textures.texture(QLatin1String(path)), rect);
    }
}

// 血量与射击间隔不再写在这里：它们来自 assets/config（见 clientconfig.cpp）。
TankPainter::TankPainter(const tankcity::config::TankStats &stats)
    : shootDelay(stats.shootDelayTicks), shootCooldown(0), health(stats.health)
{}

PlayerPainter::PlayerPainter(const tankcity::config::TankStats &stats) : TankPainter(stats)
{
    position = QPoint(0, 0);
    bodyAngle = 0; // 初始车身角度
    turretAngle = 0; // 初始炮塔角度
    shootCooldown = 0; // 初始射击冷却时间
    invincible = false;
}

void PlayerPainter::draw(QPainter &painter)
{
     painter.save();

    if (invincible) {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        // 1. 轻微透明度效果
        float alpha = 0.85f + 0.15f * sin(currentTime / 150.0f);
        painter.setOpacity(alpha);

        // 2. 动态轮廓
        float pulse = 0.9f + 0.1f * sin(currentTime / 100.0f);
        int hue = (((static_cast<int>(currentTime / 30)) % 360) + 360) % 360;

        QPen outlinePen(QColor::fromHsv(hue, 255, 255), 3 * pulse);
        painter.setPen(outlinePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(position, 25, 25);
    }

    painter.translate(position);
    painter.rotate(bodyAngle); // 使用车身角度旋转
    painter.rotate(270);        // 新增：整体顺时针旋转90°

    // 坦克车身 - 蓝色主题
    QRect bodyRect(-15, -10, 30, 20); // 尺寸缩小
    QLinearGradient bodyGrad(bodyRect.topLeft(), bodyRect.bottomRight());
    bodyGrad.setColorAt(0, QColor(80, 140, 255)); // 更亮的蓝色
    bodyGrad.setColorAt(1, QColor(40, 90, 200));  // 更深的蓝色

    painter.setPen(QPen(Qt::darkBlue, 1.5)); // 更细的边框
    painter.setBrush(bodyGrad);
    painter.drawRoundedRect(bodyRect, 4, 4); // 圆角更小

    // 坦克履带 - 深灰色，方向反转
    painter.setBrush(QBrush(QColor(50, 50, 50))); // 更深的灰色
    painter.drawRect(-18, -12, 6, 24);  // 左履带 (更窄)
    painter.drawRect(12, -12, 6, 24);   // 右履带 (更窄)

    // 履带细节 - 垂直方向
    painter.setPen(QPen(QColor(30, 30, 30), 1)); // 更深的细节色
    for (int y = -10; y <= 10; y += 5) {
        painter.drawLine(-18, y, -12, y); // 左履带细节 (垂直)
        painter.drawLine(12, y, 18, y);   // 右履带细节 (垂直)
    }

    // 炮塔基座 - 圆形
    painter.setPen(QPen(Qt::darkBlue, 1.5));
    painter.setBrush(QBrush(QColor(70, 120, 230))); // 匹配车身的蓝色
    painter.drawEllipse(-8, -8, 16, 16); // 尺寸缩小

    painter.restore();

    // 炮塔（独立于车身方向）
    painter.save();
    painter.translate(position);
    painter.rotate(turretAngle);

    // 炮管 - 更细的锥形
    painter.setPen(QPen(QColor(40, 60, 110), 4)); // 更细的炮管
    painter.drawLine(0, 0, 24, 0); // 缩短长度

    QPolygonF cannon;
    cannon << QPointF(24, -2) << QPointF(28, 0) << QPointF(24, 2); // 更小的锥形
    painter.setBrush(QBrush(QColor(60, 80, 130))); // 更深的蓝色
    painter.drawPolygon(cannon);

    // 炮口效果
    if (shootCooldown > 0 && shootCooldown >= shootDelay - 5) {
        painter.setPen(QPen(QColor(255, 220, 120), 3)); // 更亮的闪光
        painter.drawPoint(28, 0);
    }

    painter.restore();
}

EnemyPainter::EnemyPainter(int difficulty, const tankcity::config::TankStats &stats)
    : difficulty(difficulty),
        TankPainter(stats)
{
    // 血量 / 射击间隔的写死三档已删除：难度差异改由 difficulty.json 的 overrides 表达，
    // 由 client::enemyStats(difficulty) 解析后从上面传进来。
}

void EnemyPainter::draw(QPainter &painter)
{
    painter.save();
    painter.translate(position);
    painter.rotate(bodyAngle); // 使用车身角度旋转
    painter.rotate(270); 

    // 根据难度选择颜色
    QColor color;
    switch (difficulty) {
    case 0: color = QColor(220, 20, 60); break;   // 红色 - 简单
    case 1: color = QColor(255, 140, 0); break;   // 橙色 - 中等
    case 2: color = QColor(178, 34, 34); break;   // 深红 - 困难
    }

    // 坦克车身 - 菱形设计
    QPolygonF body;
    body << QPointF(0, -20) << QPointF(20, 0)
         << QPointF(0, 20) << QPointF(-20, 0);

    QLinearGradient bodyGrad(-20, 0, 20, 0);
    bodyGrad.setColorAt(0, color.lighter(150));
    bodyGrad.setColorAt(1, color.darker(150));

    painter.setPen(QPen(color.darker(200), 2));
    painter.setBrush(bodyGrad);
    painter.drawPolygon(body);

    // 坦克履带 - 深灰色
    painter.setBrush(QBrush(QColor(60, 60, 60)));
    painter.drawRect(-25, -15, 10, 30);  // 左履带
    painter.drawRect(15, -15, 10, 30);   // 右履带

    // 履带细节
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    for (int i = -10; i <= 10; i += 5) {
        painter.drawLine(-25, i, -15, i); // 左履带细节
        painter.drawLine(15, i, 25, i);   // 右履带细节
    }

    // 炮塔基座 - 方形
    painter.setPen(QPen(color.darker(200), 2));
    painter.setBrush(QBrush(color.darker(150)));
    painter.drawRect(-8, -8, 16, 16);

    painter.restore();

    // 炮塔（独立于车身方向）
    painter.save();
    painter.translate(position);
    painter.rotate(turretAngle);

    // 炮管 - 锥形
    painter.setPen(QPen(QColor(80, 30, 30), 6));
    painter.drawLine(0, 0, 30, 0);

    QPolygonF cannon;
    cannon << QPointF(30, -3) << QPointF(35, 0) << QPointF(30, 3);
    painter.setBrush(QBrush(QColor(100, 40, 40)));
    painter.drawPolygon(cannon);

    // 炮口效果
    if (shootCooldown > 0 && shootCooldown >= shootDelay - 5) {
        painter.setPen(QPen(QColor(255, 100, 0), 4));
        painter.drawPoint(35, 0);
    }

    painter.restore();

    // 绘制难度指示器
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    QString diffChar;
    switch (difficulty) {
    case 0: diffChar = "E"; break;
    case 1: diffChar = "M"; break;
    case 2: diffChar = "H"; break;
    }
    painter.drawText(position.x() - 5, position.y() - 25, diffChar);    
}

BulletPainter::BulletPainter(const QPoint &position, float angle, BulletPainterType type)
    : position(position), angle(angle), type(type)
{

}

void BulletPainter::draw(QPainter &painter) const
{
    painter.save();
    painter.translate(position);
    painter.rotate(angle); // 根据子弹角度旋转

    // 子弹主体 - 流线型设计
    QPainterPath path;
    path.moveTo(-8, 0);
    path.lineTo(6, -3);
    path.lineTo(6, 3);
    path.closeSubpath();

    // 子弹颜色
    QColor bulletColor = type == BulletPainterType::Player ?
                             QColor(100, 255, 255) : QColor(255, 100, 100);

    painter.setPen(QPen(bulletColor.darker(), 1));
    painter.setBrush(bulletColor);
    painter.drawPath(path);

    // 弹头尖端
    QPolygonF tip;
    tip << QPointF(6, -3) << QPointF(10, 0) << QPointF(6, 3);
    painter.setBrush(bulletColor.lighter(150));
    painter.drawPolygon(tip);

    painter.restore();

    // 绘制尾迹
    QPoint prevPos = position;
    prevPos.rx() -= 12 * cos(angle * M_PI / 180.0f);
    prevPos.ry() -= 12 * sin(angle * M_PI / 180.0f);

    QRadialGradient grad(prevPos, 8);
    grad.setColorAt(0, type == BulletPainterType::Player ?
                           QColor(100, 255, 255, 150) : QColor(255, 100, 100, 150));
    grad.setColorAt(1, Qt::transparent);

    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(prevPos, 6, 6);
}

