#ifndef LOADINGWIDGET_H
#define LOADINGWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QLabel>

class QProgressBar;
class QTimer;

class LoadingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoadingWidget(QWidget *parent = nullptr);

signals:
    void loadingFinished();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateProgress();

private:
    QProgressBar *progressBar;
    QTimer *timer;
    int progressValue;
    QPixmap backgroundImage;


    QWidget *progressContainer;  // 进度条总容器
    QLabel *frameLabel;          // 外边框
    QWidget *bulletContainer;    // 子弹填充容器
    QLabel *tankLabel;           // 坦克指示器

    QList<QLabel*> bullets;      // 存储所有子弹对象
    int borderThickness = 15;    // 边框厚度

};

#endif // LOADINGWIDGET_H
