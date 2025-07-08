#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QListWidget>
#include "gamewindow.h"
#include "scoreboard.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &username, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartGame();
    void onScoreboard();
    void onExit();
    void onBackToMenu();
    void onGameFinished(int score);

private:
    QString currentUsername;

    QStackedWidget *stackedWidget;


    QWidget *mainMenuWidget;
    QLabel *welcomeLabel;
    QPushButton *startButton;
    QPushButton *scoreboardButton;
    QPushButton *exitButton;


    QWidget *gameSetupWidget;
    QComboBox *mapComboBox;
    QComboBox *difficultyComboBox;
    QPushButton *startGameButton;
    QPushButton *backToMenuButton;


    GameWindow *gameWindow;


    ScoreBoard *scoreBoard;

    void setupMainMenu();
    void setupGameSetup();
};

#endif
