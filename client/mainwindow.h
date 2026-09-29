#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QPaintEvent>
#include <QStackedWidget>
#include <QListWidget>
#include "gamewindow.h"
#include "scoreboard.h"
#include <QGraphicsEffect>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &username, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartGame();
    void onExit();
    void onBackToMenu();
    void onGameFinished(int score);
    void onSinglePlayer();      // 单人闯关模式
    void onEndlessMode();       // 单人无尽模式
    void onMultiplayerMode();   // 双人联机对战模式
    void onStartMultiplayer();  // 开始双人游戏
    void showScoreBoard();      // 显示排行榜

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap backgroundImage;
    QString currentUsername;

    QStackedWidget *stackedWidget;

    // 主菜单界面
    QWidget *mainMenuWidget;
    QLabel *welcomeLabel;
    QPushButton *startButton;
    QPushButton *exitButton;

    // 模式选择界面
    QWidget *modeSelectWidget;
    QPushButton *singlePlayerButton;
    QPushButton *endlessModeButton;
    QPushButton *multiplayerButton;
    QPushButton *backToMenuButton;

    // 游戏设置界面
    QWidget *gameSetupWidget;
    QComboBox *mapComboBox;
    QComboBox *mapComboBoxMultiplayer;
    QComboBox *difficultyComboBox;
    QPushButton *startGameButton;
    QPushButton *scoreBoardButton; // 排行榜按钮

    QWidget *endlessGameSetupWidget;
    QComboBox *endlessMapComboBox;
    QComboBox *endlessMapComboBoxMultiplayer;
    QComboBox *endlessDifficultyComboBox;
    QPushButton *endlessStartGameButton;
    QPushButton *endlessScoreBoardButton; // 排行榜按钮

    // 双人联机设置界面
    QWidget *multiplayerSetupWidget;
    QLineEdit *connectionStringEdit;
    QPushButton *startMultiplayerButton;

    GameWindow *gameWindow;
    ScoreBoard *scoreBoard;

    void setupMainMenu();
    void setupModeSelect();        // 模式选择界面
    void setupGameSetup();         // 游戏设置界面
    void setupEndlessGameSetup();
    void setupMultiplayerSetup();  // 双人联机设置界面
};

class HoverEventFilter : public QObject {
    Q_OBJECT
public:
    HoverEventFilter(QGraphicsOpacityEffect* effect, QObject* parent = nullptr)
        : QObject(parent), m_effect(effect) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Enter) {
            m_effect->setOpacity(0.7);
        } else if (event->type() == QEvent::Leave) {
            m_effect->setOpacity(1.0);
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QGraphicsOpacityEffect* m_effect;
};

#endif
