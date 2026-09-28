#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QApplication>
#include <QDebug>

MainWindow::MainWindow(const QString &username, QWidget *parent)
    : QMainWindow(parent), currentUsername(username)
{
    setWindowTitle("坦克大战");
    setFixedSize(800, 600);
    setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");

    // Create data directory if not exists
    QDir dir;
    if (!dir.exists("data")) {
        dir.mkdir("data");
    }


    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    setupMainMenu();
    setupGameSetup();

    // Create game window (will be added when needed)
    gameWindow = new GameWindow(this);
    connect(gameWindow, &GameWindow::gameFinished, this, &MainWindow::onGameFinished);
    connect(gameWindow, &GameWindow::backToMenu, this, &MainWindow::onBackToMenu);

    // Create scoreboard
    scoreBoard = new ScoreBoard(this);
    connect(scoreBoard, &ScoreBoard::backToMenu, this, &MainWindow::onBackToMenu);

    stackedWidget->addWidget(mainMenuWidget);
    stackedWidget->addWidget(gameSetupWidget);
    stackedWidget->addWidget(gameWindow);
    stackedWidget->addWidget(scoreBoard);
}

MainWindow::~MainWindow() {}

void MainWindow::setupMainMenu()
{
    mainMenuWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(mainMenuWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    welcomeLabel = new QLabel("欢迎, " + currentUsername, mainMenuWidget);
    welcomeLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #3498db;");
    welcomeLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("坦克大战", mainMenuWidget);
    titleLabel->setStyleSheet("font-size: 48px; font-weight: bold; color: #e74c3c;");
    titleLabel->setAlignment(Qt::AlignCenter);

    startButton = new QPushButton("开始游戏", mainMenuWidget);
    startButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; padding: 15px; font-size: 18px; font-weight: bold; }"
                               "QPushButton:hover { background-color: #2ecc71; }");
    startButton->setFixedSize(200, 50);

    scoreboardButton = new QPushButton("排行榜", mainMenuWidget);
    scoreboardButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; border-radius: 5px; padding: 15px; font-size: 18px; font-weight: bold; }"
                                    "QPushButton:hover { background-color: #2980b9; }");
    scoreboardButton->setFixedSize(200, 50);

    exitButton = new QPushButton("退出游戏", mainMenuWidget);
    exitButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border-radius: 5px; padding: 15px; font-size: 18px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #c0392b; }");
    exitButton->setFixedSize(200, 50);

    layout->addWidget(welcomeLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(startButton);
    layout->addWidget(scoreboardButton);
    layout->addWidget(exitButton);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartGame);
    connect(scoreboardButton, &QPushButton::clicked, this, &MainWindow::onScoreboard);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExit);

    stackedWidget->addWidget(mainMenuWidget);
}

void MainWindow::setupGameSetup()
{
    gameSetupWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(gameSetupWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    QLabel *titleLabel = new QLabel("游戏设置", gameSetupWidget);
    titleLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #3498db;");
    titleLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *mapLayout = new QHBoxLayout();
    QLabel *mapLabel = new QLabel("选择地图:", gameSetupWidget);
    mapLabel->setStyleSheet("font-size: 18px;");
    mapComboBox = new QComboBox(gameSetupWidget);
    mapComboBox->addItem("地图 1 - 平原");
    mapComboBox->addItem("地图 2 - 城市");
    mapComboBox->addItem("地图 3 - 沙漠");
    mapComboBox->addItem("地图 4 - 雪地");
    mapComboBox->addItem("地图 5 - 丛林");
    mapComboBox->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");
    mapComboBox->setFixedSize(300, 40);
    mapLayout->addWidget(mapLabel);
    mapLayout->addWidget(mapComboBox);

    QHBoxLayout *difficultyLayout = new QHBoxLayout();
    QLabel *difficultyLabel = new QLabel("选择难度:", gameSetupWidget);
    difficultyLabel->setStyleSheet("font-size: 18px;");
    difficultyComboBox = new QComboBox(gameSetupWidget);
    difficultyComboBox->addItem("简单");
    difficultyComboBox->addItem("中等");
    difficultyComboBox->addItem("困难");
    difficultyComboBox->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");
    difficultyComboBox->setFixedSize(300, 40);
    difficultyLayout->addWidget(difficultyLabel);
    difficultyLayout->addWidget(difficultyComboBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    startGameButton = new QPushButton("开始游戏", gameSetupWidget);
    startGameButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; padding: 15px; font-size: 18px; font-weight: bold; }"
                                   "QPushButton:hover { background-color: #2ecc71; }");
    startGameButton->setFixedSize(150, 50);

    backToMenuButton = new QPushButton("返回主菜单", gameSetupWidget);
    backToMenuButton->setStyleSheet("QPushButton { background-color: #e67e22; color: white; border-radius: 5px; padding: 15px; font-size: 18px; font-weight: bold; }"
                                    "QPushButton:hover { background-color: #d35400; }");
    backToMenuButton->setFixedSize(150, 50);

    buttonLayout->addWidget(startGameButton);
    buttonLayout->addWidget(backToMenuButton);

    layout->addWidget(titleLabel);
    layout->addLayout(mapLayout);
    layout->addLayout(difficultyLayout);
    layout->addLayout(buttonLayout);

    connect(startGameButton, &QPushButton::clicked, this, [this]() {
        int mapIndex = mapComboBox->currentIndex();
        int difficulty = difficultyComboBox->currentIndex();
        gameWindow->startGame(mapIndex, difficulty);
        stackedWidget->setCurrentWidget(gameWindow);
    });

    connect(backToMenuButton, &QPushButton::clicked, this, &MainWindow::onBackToMenu);
}

void MainWindow::onStartGame()
{
    stackedWidget->setCurrentWidget(gameSetupWidget);
    gameSetupWidget->setFocus();
}

void MainWindow::onScoreboard()
{
    scoreBoard->loadScores();
    stackedWidget->setCurrentWidget(scoreBoard);
}

void MainWindow::onExit()
{
    QApplication::quit();
}

void MainWindow::onBackToMenu()
{
    stackedWidget->setCurrentWidget(mainMenuWidget);
}

void MainWindow::onGameFinished(int score)
{
    // Save score
    QFile file("data/scores.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << currentUsername << ":" << score << "\n";
        file.close();
    }

    QMessageBox::information(this, "游戏结束", QString("游戏结束! 你的得分: %1").arg(score));
    stackedWidget->setCurrentWidget(mainMenuWidget);
    mainMenuWidget->setFocus();
}
