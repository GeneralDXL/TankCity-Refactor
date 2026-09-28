#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QLineEdit>

MainWindow::MainWindow(const QString &username, QWidget *parent)
    : QMainWindow(parent), currentUsername(username)
{
    backgroundImage.load("./../../assets/images/ui/theme.png");
    if (backgroundImage.isNull()) {
        qWarning() << "Failed to load background image";
        setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");  // 回退样式
    } else {
        setStyleSheet("color: #ecf0f1;");  // 只保留文字颜色样式
    }
    setWindowTitle("坦克大战");
    setFixedSize(1200, 900);
    // 创建数据目录
    QDir dir;
    if (!dir.exists("data")) {
        dir.mkdir("data");
    }

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    setupMainMenu();            // 主菜单界面
    setupModeSelect();          // 模式选择界面
    setupGameSetup();           // 游戏设置界面
    setupEndlessGameSetup();
    setupMultiplayerSetup();    // 双人联机设置界面

    // 创建游戏窗口
    gameWindow = new GameWindow(this);
    connect(gameWindow, &GameWindow::gameFinished, this, &MainWindow::onGameFinished);
    connect(gameWindow, &GameWindow::backToMenu, this, &MainWindow::onBackToMenu);

    // 创建排行榜
    scoreBoard = new ScoreBoard(this);
    connect(scoreBoard, &ScoreBoard::backToMenu, this, &MainWindow::onBackToMenu);

    // 添加所有界面到堆栈
    stackedWidget->addWidget(mainMenuWidget);
    stackedWidget->addWidget(modeSelectWidget);
    stackedWidget->addWidget(gameSetupWidget);
    stackedWidget->addWidget(endlessGameSetupWidget);
    stackedWidget->addWidget(multiplayerSetupWidget);
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

    QLabel *titleLabel = new QLabel(mainMenuWidget);
    QPixmap titlePixmap("./../../assets/images/ui/title.png");
    if(!titlePixmap.isNull())
    {
        titleLabel->setPixmap(titlePixmap.scaledToWidth(600,Qt::SmoothTransformation));
        titleLabel->setAlignment(Qt::AlignCenter);
    }
    else
    {
        qWarning() << "Failed to load title image";
        titleLabel->setText("坦克大战");
        titleLabel->setStyleSheet(
            "font-size: 72px;"
            "font-weight: bold;"
            "color: #e74c3c;"
            "font-family: '华文行楷';"
            "text-shadow: 3px 3px 6px rgba(0,0,0,0.4);"
            );
    }
    titleLabel->setAlignment(Qt::AlignCenter);

    auto createImageButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(mainMenuWidget);
        QPixmap pix(path);
        if (!pix.isNull()) {
            // 按比例缩放至指定宽度，高度自动计算
            btn->setIcon(QIcon(pix.scaledToWidth(width, Qt::SmoothTransformation)));
            btn->setIconSize(pix.scaledToWidth(width, Qt::SmoothTransformation).size());
        }
        btn->setFixedSize(width, pix.height() * width / pix.width()); // 保持比例
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QPushButton:hover {"
            "   opacity: 0.7;"  // 悬停时70%不透明度
            "}"
            );

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(btn);
        effect->setOpacity(1.0);
        btn->setGraphicsEffect(effect);

        btn->installEventFilter(new HoverEventFilter(effect, btn));

        btn->setFixedSize(width, pix.height() * width / pix.width());
        return btn;
    };

    const int BUTTON_WIDTH = 300;

    startButton = createImageButton("./../../assets/images/ui/start.png", BUTTON_WIDTH);
    exitButton = createImageButton("./../../assets/images/ui/exit.png", BUTTON_WIDTH);


    layout->addWidget(titleLabel,0,Qt::AlignCenter);
    layout->addWidget(startButton,0,Qt::AlignCenter);
    layout->addWidget(exitButton,0,Qt::AlignCenter);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartGame);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExit);

    stackedWidget->addWidget(mainMenuWidget);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!backgroundImage.isNull()) {
        QPainter painter(this);
        // 等比例缩放填充
        QPixmap scaled = backgroundImage.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap((width() - scaled.width())/2, (height() - scaled.height())/2, scaled);

        // 添加半透明遮罩增强文字可读性
        painter.fillRect(rect(), QColor(0, 0, 0, 60));
    }

    QMainWindow::paintEvent(event);  // 确保原有绘制继续执行
}

void MainWindow::setupModeSelect()
{
    modeSelectWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(modeSelectWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    auto createImageButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(mainMenuWidget);
        QPixmap pix(path);
        if (!pix.isNull()) {
            // 按比例缩放至指定宽度，高度自动计算
            btn->setIcon(QIcon(pix.scaledToWidth(width, Qt::SmoothTransformation)));
            btn->setIconSize(pix.scaledToWidth(width, Qt::SmoothTransformation).size());
        }
        btn->setFixedSize(width, pix.height() * width / pix.width()); // 保持比例
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QPushButton:hover {"
            "   opacity: 0.7;"  // 悬停时70%不透明度
            "}"
            );

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(btn);
        effect->setOpacity(1.0);
        btn->setGraphicsEffect(effect);

        btn->installEventFilter(new HoverEventFilter(effect, btn));

        btn->setFixedSize(width, pix.height() * width / pix.width());
        return btn;
    };

    QLabel *titleLabel = new QLabel(modeSelectWidget);
    QPixmap scoreboardPixmap("./../../assets/images/ui/select.png");
    titleLabel->setPixmap(scoreboardPixmap.scaledToWidth(400, Qt::SmoothTransformation));
    titleLabel->setAlignment(Qt::AlignCenter);

    const int BUTTON_WIDTH=250;

    // 单人闯关模式按钮
    singlePlayerButton = createImageButton("./../../assets/images/ui/single_lim.png", BUTTON_WIDTH);

    // 单人无尽模式按钮
    endlessModeButton = createImageButton("./../../assets/images/ui/single_inf.png", BUTTON_WIDTH);

    // 双人联机对战按钮
    multiplayerButton = createImageButton("./../../assets/images/ui/multi.png", BUTTON_WIDTH);

    // 返回主菜单按钮
    backToMenuButton = createImageButton("./../../assets/images/ui/return.png", 150);

    layout->addWidget(titleLabel,0,Qt::AlignCenter);
    layout->addWidget(singlePlayerButton,0,Qt::AlignCenter);
    layout->addWidget(endlessModeButton,0,Qt::AlignCenter);
    layout->addWidget(multiplayerButton,0,Qt::AlignCenter);
    layout->addWidget(backToMenuButton, 0, Qt::AlignCenter);

    // 连接按钮信号
    connect(singlePlayerButton, &QPushButton::clicked, this, &MainWindow::onSinglePlayer);
    connect(endlessModeButton, &QPushButton::clicked, this, &MainWindow::onEndlessMode);
    connect(multiplayerButton, &QPushButton::clicked, this, &MainWindow::onMultiplayerMode);
    connect(backToMenuButton, &QPushButton::clicked, this, &MainWindow::onBackToMenu);
}

void MainWindow::setupGameSetup()
{
    gameSetupWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(gameSetupWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    QLabel *titleLabel = new QLabel(gameSetupWidget);
    QPixmap setupPixmap("./../../assets/images/ui/setting.png");
    if(!setupPixmap.isNull()) {
        titleLabel->setPixmap(setupPixmap.scaledToWidth(300, Qt::SmoothTransformation));
        titleLabel->setAlignment(Qt::AlignCenter);
    } else {
        titleLabel->setText("游戏设置");
        titleLabel->setStyleSheet(
            "font-size: 48px;"
            "color: #3498db;"
            "font-family: '华文行楷';"
            "text-shadow: 2px 2px 4px rgba(0,0,0,0.3);"
            );
    }

    QHBoxLayout *mapLayout = new QHBoxLayout();
    QLabel *mapLabel = new QLabel("选择地图:", gameSetupWidget);
    mapLabel->setStyleSheet("font-size: 18px;");
    mapComboBox = new QComboBox(gameSetupWidget);
    mapComboBox->clear();
    mapComboBox->addItem("地图 1 - 钢铁走廊");
    mapComboBox->addItem("地图 2 - 钢铁迷宫 ");
    mapComboBox->addItem("地图 3 - 钢铁堡垒 ");
    mapComboBox->addItem("地图 4 - 突破防线");
    mapComboBox->addItem("地图 5 - 密林伏击");
    mapComboBox->addItem("地图 6 - 冰封战场");
    mapComboBox->addItem("地图 7 - 海峡封锁");
    mapComboBox->addItem("地图 8 - 天险通道");
    mapComboBox->addItem("地图 9 - 森林迷宫");
    mapComboBox->addItem("地图 10 -环岛决战");
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

    auto createSetupButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(gameSetupWidget);
        QPixmap pix(path);
        if (!pix.isNull()) {
            btn->setIcon(QIcon(pix.scaledToWidth(width, Qt::SmoothTransformation)));
            btn->setIconSize(pix.scaledToWidth(width, Qt::SmoothTransformation).size());
        }
        btn->setFixedSize(width, pix.height() * width / pix.width());
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QPushButton:hover {"
            "   opacity: 0.7;"  // 悬停时70%不透明度
            "}"
            );

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(btn);
        effect->setOpacity(1.0);
        btn->setGraphicsEffect(effect);

        btn->installEventFilter(new HoverEventFilter(effect, btn));
        return btn;
    };

    const int SETUP_BUTTON_WIDTH =200;

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    startGameButton = createSetupButton("./../../assets/images/ui/start.png", SETUP_BUTTON_WIDTH);
    backToMenuButton = createSetupButton("./../../assets/images/ui/return.png", SETUP_BUTTON_WIDTH);
    scoreBoardButton = createSetupButton("./../../assets/images/ui/row.png", SETUP_BUTTON_WIDTH);

    buttonLayout->addWidget(startGameButton);
    buttonLayout->addWidget(scoreBoardButton);
    buttonLayout->addWidget(backToMenuButton);

    layout->addWidget(titleLabel);
    layout->addLayout(mapLayout);
    layout->addLayout(difficultyLayout);
    layout->addLayout(buttonLayout);

    // 连接按钮信号
    connect(startGameButton, &QPushButton::clicked, this, [this]() {
        int mapIndex = mapComboBox->currentIndex();
        int difficulty = difficultyComboBox->currentIndex();
        gameWindow->startGame(mapIndex, difficulty, 0);
        stackedWidget->setCurrentWidget(gameWindow);
    });

    connect(scoreBoardButton, &QPushButton::clicked, this, &MainWindow::showScoreBoard);
    connect(backToMenuButton, &QPushButton::clicked, this, &MainWindow::onBackToMenu);
}

void MainWindow::setupEndlessGameSetup()
{
    endlessGameSetupWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(endlessGameSetupWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    QLabel *titleLabel = new QLabel(endlessGameSetupWidget);
    QPixmap setupPixmap("./../../assets/images/ui/setting.png");
    if(!setupPixmap.isNull()) {
        titleLabel->setPixmap(setupPixmap.scaledToWidth(300, Qt::SmoothTransformation));
        titleLabel->setAlignment(Qt::AlignCenter);
    } else {
        titleLabel->setText("游戏设置");
        titleLabel->setStyleSheet(
            "font-size: 48px;"
            "color: #3498db;"
            "font-family: '华文行楷';"
            "text-shadow: 2px 2px 4px rgba(0,0,0,0.3);"
            );
    }

    QHBoxLayout *mapLayout = new QHBoxLayout();
    QLabel *mapLabel = new QLabel("选择地图:", endlessGameSetupWidget);
    mapLabel->setStyleSheet("font-size: 18px;");
    endlessMapComboBox = new QComboBox(endlessGameSetupWidget);
    endlessMapComboBox->addItem("地图 1 - 钢铁走廊");
    endlessMapComboBox->addItem("地图 2 - 钢铁迷宫 ");
    endlessMapComboBox->addItem("地图 3 - 钢铁堡垒 ");
    endlessMapComboBox->addItem("地图 4 - 突破防线");
    endlessMapComboBox->addItem("地图 5 - 密林伏击");
    endlessMapComboBox->addItem("地图 6 - 冰封战场");
    endlessMapComboBox->addItem("地图 7 - 海峡封锁");
    endlessMapComboBox->addItem("地图 8 - 天险通道");
    endlessMapComboBox->addItem("地图 9 - 森林迷宫");
    endlessMapComboBox->addItem("地图 10 -环岛决战");
    endlessMapComboBox->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");
    endlessMapComboBox->setFixedSize(300, 40);
    mapLayout->addWidget(mapLabel);
    mapLayout->addWidget(endlessMapComboBox);

    QHBoxLayout *difficultyLayout = new QHBoxLayout();
    QLabel *difficultyLabel = new QLabel("选择难度:", endlessGameSetupWidget);
    difficultyLabel->setStyleSheet("font-size: 18px;");
    endlessDifficultyComboBox = new QComboBox(endlessGameSetupWidget);
    endlessDifficultyComboBox->addItem("简单");
    endlessDifficultyComboBox->addItem("中等");
    endlessDifficultyComboBox->addItem("困难");
    endlessDifficultyComboBox->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");
    endlessDifficultyComboBox->setFixedSize(300, 40);
    difficultyLayout->addWidget(difficultyLabel);
    difficultyLayout->addWidget(endlessDifficultyComboBox);

    auto createSetupButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(endlessGameSetupWidget);
        QPixmap pix(path);
        if (!pix.isNull()) {
            btn->setIcon(QIcon(pix.scaledToWidth(width, Qt::SmoothTransformation)));
            btn->setIconSize(pix.scaledToWidth(width, Qt::SmoothTransformation).size());
        }
        btn->setFixedSize(width, pix.height() * width / pix.width());
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QPushButton:hover {"
            "   opacity: 0.7;"  // 悬停时70%不透明度
            "}"
            );

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(btn);
        effect->setOpacity(1.0);
        btn->setGraphicsEffect(effect);

        btn->installEventFilter(new HoverEventFilter(effect, btn));
        return btn;
    };

    const int SETUP_BUTTON_WIDTH =200;

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    endlessStartGameButton = createSetupButton("./../../assets/images/ui/start.png", SETUP_BUTTON_WIDTH);
    backToMenuButton = createSetupButton("./../../assets/images/ui/return.png", SETUP_BUTTON_WIDTH);
    endlessScoreBoardButton = createSetupButton("./../../assets/images/ui/row.png", SETUP_BUTTON_WIDTH);

    buttonLayout->addWidget(endlessStartGameButton);
    buttonLayout->addWidget(endlessScoreBoardButton);
    buttonLayout->addWidget(backToMenuButton);

    layout->addWidget(titleLabel);
    layout->addLayout(mapLayout);
    layout->addLayout(difficultyLayout);
    layout->addLayout(buttonLayout);

    // 连接按钮信号
    connect(endlessStartGameButton, &QPushButton::clicked, this, [this]() {
        int mapIndex = endlessMapComboBox->currentIndex();
        int difficulty = endlessDifficultyComboBox->currentIndex();
        gameWindow->startGame(mapIndex, difficulty, 1);
        stackedWidget->setCurrentWidget(gameWindow);
    });

    connect(endlessScoreBoardButton, &QPushButton::clicked, this, &MainWindow::showScoreBoard);
    connect(backToMenuButton, &QPushButton::clicked, this, &MainWindow::onBackToMenu);
}

void MainWindow::setupMultiplayerSetup()
{
    multiplayerSetupWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(multiplayerSetupWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    QLabel *titleLabel = new QLabel(multiplayerSetupWidget);
    QPixmap scoreboardPixmap("./../../assets/images/ui/multi.png");
    titleLabel->setPixmap(scoreboardPixmap.scaledToWidth(400, Qt::SmoothTransformation));
    titleLabel->setAlignment(Qt::AlignCenter);

    // +++ 新增地图选择部分 +++
    QHBoxLayout *mapLayout = new QHBoxLayout();
    QLabel *mapLabel = new QLabel("选择地图:", multiplayerSetupWidget);
    mapLabel->setStyleSheet("font-size: 18px;");
    mapComboBoxMultiplayer = new QComboBox(multiplayerSetupWidget);  // 使用专用下拉框
    mapComboBoxMultiplayer->addItem("地图 1 - 钢铁走廊");
    mapComboBoxMultiplayer->addItem("地图 2 - 钢铁迷宫 ");
    mapComboBoxMultiplayer->addItem("地图 3 - 钢铁堡垒 ");
    mapComboBoxMultiplayer->addItem("地图 4 - 突破防线");
    mapComboBoxMultiplayer->addItem("地图 5 - 密林伏击");
    mapComboBoxMultiplayer->addItem("地图 6 - 冰封战场");
    mapComboBoxMultiplayer->addItem("地图 7 - 海峡封锁");
    mapComboBoxMultiplayer->addItem("地图 8 - 天险通道");
    mapComboBoxMultiplayer->addItem("地图 9 - 森林迷宫");
    mapComboBoxMultiplayer->addItem("地图 10 -环岛决战");
    mapComboBoxMultiplayer->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");
    mapComboBoxMultiplayer->setFixedSize(300, 40);
    mapLayout->addWidget(mapLabel);
    mapLayout->addWidget(mapComboBoxMultiplayer);
    // +++ 地图选择结束 +++

    QLabel *connectionLabel = new QLabel("面对面进入游戏:", multiplayerSetupWidget);
    connectionLabel->setStyleSheet("font-size: 18px; color: #ecf0f1;");
    connectionLabel->setAlignment(Qt::AlignCenter);

    connectionStringEdit = new QLineEdit(multiplayerSetupWidget);
    connectionStringEdit->setPlaceholderText("输入连接代码");
    connectionStringEdit->setAlignment(Qt::AlignCenter);
    connectionStringEdit->setStyleSheet("padding: 10px; border-radius: 5px; background-color: #34495e; color: #ecf0f1; font-size: 16px;");
    connectionStringEdit->setFixedSize(300, 40);

    auto createImageButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(mainMenuWidget);
        QPixmap pix(path);
        if (!pix.isNull()) {
            // 按比例缩放至指定宽度，高度自动计算
            btn->setIcon(QIcon(pix.scaledToWidth(width, Qt::SmoothTransformation)));
            btn->setIconSize(pix.scaledToWidth(width, Qt::SmoothTransformation).size());
        }
        btn->setFixedSize(width, pix.height() * width / pix.width()); // 保持比例
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QPushButton:hover {"
            "   opacity: 0.7;"  // 悬停时70%不透明度
            "}"
            );

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(btn);
        effect->setOpacity(1.0);
        btn->setGraphicsEffect(effect);

        btn->installEventFilter(new HoverEventFilter(effect, btn));

        btn->setFixedSize(width, pix.height() * width / pix.width());
        return btn;
    };

    const int BUTTON_WIDTH=200;

    startMultiplayerButton = createImageButton("./../../assets/images/ui/start.png", BUTTON_WIDTH);

    // 返回按钮
    QPushButton *backToMenuButton = createImageButton("./../../assets/images/ui/return.png", BUTTON_WIDTH);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(startMultiplayerButton);
    buttonLayout->addWidget(backToMenuButton);

    // 添加地图选择到布局
    layout->addWidget(titleLabel);
    layout->addLayout(mapLayout);  // 将地图选择添加到顶部
    layout->addSpacing(20);
    layout->addWidget(connectionLabel);
    layout->addWidget(connectionStringEdit, 0, Qt::AlignCenter);
    layout->addSpacing(30);
    layout->addLayout(buttonLayout);

    connect(startMultiplayerButton, &QPushButton::clicked, this, &MainWindow::onStartMultiplayer);
    connect(backToMenuButton, &QPushButton::clicked, this, &MainWindow::onBackToMenu);
}

void MainWindow::onStartGame()
{
    stackedWidget->setCurrentWidget(modeSelectWidget);
    gameSetupWidget->setFocus();
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
    // 保存分数
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

void MainWindow::onSinglePlayer()
{
    stackedWidget->setCurrentWidget(gameSetupWidget);
}

void MainWindow::onEndlessMode()
{
    stackedWidget->setCurrentWidget(endlessGameSetupWidget);
}

void MainWindow::onMultiplayerMode()
{
    stackedWidget->setCurrentWidget(multiplayerSetupWidget);
    connectionStringEdit->setFocus();
}

void MainWindow::onStartMultiplayer()
{
    QString connectionString = connectionStringEdit->text().trimmed();
    if (connectionString.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入连接代码!");
        return;
    }

    int selectedMap = mapComboBoxMultiplayer->currentIndex();

    gameWindow->startGame(selectedMap, 1, 2); // 假设地图0，中等难度
    stackedWidget->setCurrentWidget(gameWindow);
}

void MainWindow::showScoreBoard()
{
    scoreBoard->loadScores();
    stackedWidget->setCurrentWidget(scoreBoard);
}
