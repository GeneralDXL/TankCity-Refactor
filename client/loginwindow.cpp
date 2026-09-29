#include "loginwindow.h"
#include "mainwindow.h"
#include <QDir>
#include <QDebug>
#include <QPixmap>


// 静态成员初始化
QString LoginWindow::accountsFilePath = "data/accounts.txt";

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    backgroundImage.load("./../../assets/images/ui/login.png");
    if (backgroundImage.isNull()) {
        qDebug() << "Failed to load background image";
        setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");  // 回退样式
    } else {
        qDebug()<<"loaded successfully";
        setStyleSheet("color: #ecf0f1;");  // 只保留文字颜色样式
    }
    // 创建数据目录
    QDir dir;
    if (!dir.exists("data")) {
        dir.mkdir("data");
    }

    setWindowTitle("坦克大战 - 登录");
    setFixedSize(800, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(20);

    auto createImageButton = [this](const QString& path, int width) {
        QPushButton* btn = new QPushButton(this);
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

    QLabel *usernameLabel = new QLabel("用户名:", this);
    usernameLabel->setStyleSheet("font-size: 16px;");
    usernameEdit = new QLineEdit(this);
    usernameEdit->setPlaceholderText("输入用户名");
    usernameEdit->setFixedWidth(400);
    usernameEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QLabel *passwordLabel = new QLabel("密码:", this);
    passwordLabel->setStyleSheet("font-size: 16px;");
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("输入密码");
    passwordEdit->setFixedWidth(400);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    loginButton = createImageButton("./../../assets/images/ui/sign_in.png", BUTTON_WIDTH);
    registerButton = createImageButton("./../../assets/images/ui/sign_up.png", BUTTON_WIDTH);

    buttonLayout->addWidget(loginButton);
    buttonLayout->addWidget(registerButton);

    mainLayout->addWidget(usernameLabel,0,Qt::AlignCenter);
    mainLayout->addWidget(usernameEdit,0,Qt::AlignCenter);
    mainLayout->addWidget(passwordLabel,0,Qt::AlignCenter);
    mainLayout->addWidget(passwordEdit,0,Qt::AlignCenter);
    mainLayout->addLayout(buttonLayout);

    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
}

LoginWindow::~LoginWindow() {}

void LoginWindow::onLoginClicked()
{
    QString username = usernameEdit->text();
    QString password = passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "错误", "用户名和密码不能为空!");
        return;
    }

    if (validateCredentials(username, password)) {
        MainWindow *mainWindow = new MainWindow(username);
        mainWindow->show();
        this->close();
    } else {
        QMessageBox::warning(this, "错误", "用户名或密码不正确!");
    }
}

void LoginWindow::onRegisterClicked()
{
    RegisterWindow *registerWindow = new RegisterWindow();
    registerWindow->show();
    this->hide(); // 隐藏当前窗口

    connect(registerWindow, &RegisterWindow::backToLogin, this, [this]() {
        this->show();
    });

    connect(registerWindow, &RegisterWindow::registrationSuccess,
            this, &LoginWindow::onRegistrationSuccess);
}

void LoginWindow::onRegistrationSuccess(const QString &username, const QString &password)
{
    usernameEdit->setText(username);
    passwordEdit->setText(password);
    QMessageBox::information(this, "成功", "注册成功! 请登录。");
}

// 静态方法实现
bool LoginWindow::validateCredentials(const QString &username, const QString &password)
{
    QString filePath = "data/accounts.txt";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开账户文件:" << file.errorString();
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QStringList parts = line.split(":");
        if (parts.size() == 2) {
            QString storedUsername = parts[0].trimmed();
            QString storedPassword = parts[1].trimmed();

            if (storedUsername == username) {
                if (password.isEmpty() || storedPassword == password) {
                    file.close();
                    return true;
                }
            }
        }
    }

    file.close();
    return false;
}

void LoginWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (!backgroundImage.isNull()) {
        // 等比例缩放填充窗口
        QPixmap scaled = backgroundImage.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, scaled);
    } else {
        painter.fillRect(rect(), QColor("#2c3e50")); // 回退背景色
    }
    QWidget::paintEvent(event);
}
void LoginWindow::saveCredentials(const QString &username, const QString &password)
{
    QString filePath = "data/accounts.txt";
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法保存账户信息:" << file.errorString();
        return;
    }

    QTextStream out(&file);
    out << username << ":" << password << "\n";
    file.close();

    qDebug() << "账户已保存:" << username;
}



