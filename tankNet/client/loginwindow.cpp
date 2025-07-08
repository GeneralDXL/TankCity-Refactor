#include "loginwindow.h"
#include "mainwindow.h"
#include <QDir>
#include <QDebug>


// 静态成员初始化
QString LoginWindow::accountsFilePath = "data/accounts.txt";

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    // 创建数据目录
    QDir dir;
    if (!dir.exists("data")) {
        dir.mkdir("data");
    }

    setWindowTitle("坦克大战 - 登录");
    setFixedSize(400, 300);
    setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(20);

    QLabel *titleLabel = new QLabel("坦克大战", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #e74c3c; margin-bottom: 20px;");

    QLabel *usernameLabel = new QLabel("用户名:", this);
    usernameLabel->setStyleSheet("font-size: 16px;");
    usernameEdit = new QLineEdit(this);
    usernameEdit->setPlaceholderText("输入用户名");
    usernameEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QLabel *passwordLabel = new QLabel("密码:", this);
    passwordLabel->setStyleSheet("font-size: 16px;");
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("输入密码");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    loginButton = new QPushButton("登录", this);
    loginButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; padding: 10px; font-weight: bold; }"
                               "QPushButton:hover { background-color: #2ecc71; }");
    registerButton = new QPushButton("注册", this);
    registerButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; border-radius: 5px; padding: 10px; font-weight: bold; }"
                                  "QPushButton:hover { background-color: #2980b9; }");

    buttonLayout->addWidget(loginButton);
    buttonLayout->addWidget(registerButton);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(usernameLabel);
    mainLayout->addWidget(usernameEdit);
    mainLayout->addWidget(passwordLabel);
    mainLayout->addWidget(passwordEdit);
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
