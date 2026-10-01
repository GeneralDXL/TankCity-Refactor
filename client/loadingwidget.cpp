#include "loadingwidget.h"
#include "asset/AssetPaths.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include<QPixmap>
#include<QLabel>
#include <qprogressbar.h>
#include <QPropertyAnimation>

LoadingWidget::LoadingWidget(QWidget *parent)
    : QWidget(parent), progressValue(0)
{
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(1450, 800); // 固定窗口大小

    // 尝试加载外部图片
    QString imagePath = engine::asset::assetPath("images/ui/theme.png");
    if (QFileInfo::exists(imagePath)) {
        if (!backgroundImage.load(imagePath)) {
            qWarning() << "Failed to load image from:" << imagePath;
        }
    } else {
        qWarning() << "Image file not found at:" << imagePath;
    }

    // 进度条设置
    QPixmap bulletPix(engine::asset::assetPath("images/ui/bullet.png"));
    QPixmap tankPix(engine::asset::assetPath("images/ui/tank.png"));
    QPixmap framePix(engine::asset::assetPath("images/ui/framework.png"));

    progressContainer=new QWidget(this);
    progressContainer->setFixedSize(1400,60);
    framePix = framePix.scaled(progressContainer->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    bulletPix = bulletPix.scaled(
        129,
        351,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
    tankPix = tankPix.scaled(
        1143,
        767,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
    tankPix=tankPix.scaledToHeight(progressContainer->height()*0.8,Qt::SmoothTransformation);

    frameLabel=new QLabel(progressContainer);
    frameLabel->setPixmap(framePix);
    frameLabel->setAlignment(Qt::AlignCenter);

    const int borderThickness=15;

    QRect progressArea(
        borderThickness,
        borderThickness,
        framePix.width() - 2*borderThickness,
        framePix.height() - 2*borderThickness
        );

    progressBar = new QProgressBar(progressContainer);
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(false);
    progressBar->setGeometry(progressArea);
    progressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background: transparent;
        }
        QProgressBar::chunk {
            background: transparent;
            width: )" + QString::number(bulletPix.width()) + "px;" +
        R"(}
        )");

    // 创建子弹填充效果
    bulletContainer = new QWidget(progressBar);
    bulletContainer->setGeometry(0, 0, progressArea.width(), progressArea.height());
    bulletContainer->setStyleSheet("background: transparent;");

    //坦克图标跟随进度
    tankLabel = new QLabel(progressBar);
    tankLabel->setPixmap(tankPix);
    tankLabel->move(0, (progressArea.height() - tankPix.height())/2);

    // 布局设置
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(progressBar, 0, Qt::AlignHCenter);
    layout->setContentsMargins(50, 0, 50, 150); // 底部间距150px
    progressContainer->move(progressBar->x(),605);

    // 定时器设置
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &LoadingWidget::updateProgress);
    timer->start(50);
}

void LoadingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    if (!backgroundImage.isNull()) {
        // 等比例缩放填充
        QPixmap scaled = backgroundImage.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap((width() - scaled.width())/2, (height() - scaled.height())/2, scaled);

        // 添加半透明遮罩
        painter.fillRect(rect(), QColor(0, 0, 0, 60));
    } else {
        // 备用纯色背景
        painter.fillRect(rect(), QColor("#2c3e50"));
    }
}

void LoadingWidget::updateProgress()
{
    progressValue = qMin(progressValue + 1, 100);
    progressBar->setValue(progressValue);

    // 计算坦克位置（考虑缩放后尺寸）
    int tankX = (progressBar->width() - tankLabel->width()) * progressValue / 100;

    // 边界保护
    tankX = qBound(0, tankX, progressBar->width() - tankLabel->width());

    // 垂直居中（考虑进度条边框）
    int tankY = (progressBar->height() - tankLabel->height()) / 2;

    tankLabel->move(tankX, tankY);

    QPixmap bulletPix(engine::asset::assetPath("images/ui/bullet.png"));
    bulletPix = bulletPix.scaled(
        10,
        bulletContainer->height()*0.8,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
    // 添加子弹（每2%添加一个）
    if (progressValue % 2 == 0) {
        QLabel *bullet = new QLabel(progressBar);
        bullet->setPixmap(bulletPix);

        // 子弹位置（从坦克尾部出现）
        int bulletX = tankX + tankLabel->width() - bullet->width();
        int bulletY = (progressBar->height() - bullet->height()) / 2;

        bullet->move(bulletX, bulletY);
        bullet->show();
        bullets.append(bullet);
    }

    // 清理超出屏幕的子弹
    while(!bullets.isEmpty() && bullets.first()->x() < 0) {
        delete bullets.takeFirst();
    }

    if(progressValue>=100)
    {
        timer->stop();
        // 淡出动画
        QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
        anim->setDuration(500);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        anim->start(QPropertyAnimation::DeleteWhenStopped);

        connect(anim, &QPropertyAnimation::finished, [this]() {
            emit loadingFinished();
            this->deleteLater();
        });
    }
}
