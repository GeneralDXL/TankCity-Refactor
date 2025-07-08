#include "registerwindow.h"
#include "loginwindow.h"

RegisterWindow::RegisterWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("注册");
    setFixedSize(400, 300);
    setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(20);

    QLabel *titleLabel = new QLabel("注册新账户", this);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #3498db;");
    titleLabel->setAlignment(Qt::AlignCenter);

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
    registerButton = new QPushButton("注册", this);
    registerButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border-radius: 5px; padding: 10px; font-weight: bold; }"
                                  "QPushButton:hover { background-color: #2ecc71; }");

    backButton = new QPushButton("返回", this);
    backButton->setStyleSheet("QPushButton { background-color: #e67e22; color: white; border-radius: 5px; padding: 10px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #d35400; }");

    buttonLayout->addWidget(registerButton);
    buttonLayout->addWidget(backButton);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(usernameLabel);
    mainLayout->addWidget(usernameEdit);
    mainLayout->addWidget(passwordLabel);
    mainLayout->addWidget(passwordEdit);
    mainLayout->addLayout(buttonLayout);

    connect(registerButton, &QPushButton::clicked, this, &RegisterWindow::onRegisterClicked);
    connect(backButton, &QPushButton::clicked, this, [this]() { this->close(); });
}

void RegisterWindow::onRegisterClicked()
{
    QString username = usernameEdit->text();
    QString password = passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "错误", "用户名和密码不能为空!");
        return;
    }

    if (LoginWindow::validateCredentials(username, "")) {
        QMessageBox::warning(this, "错误", "用户名已存在!");
        return;
    }

    LoginWindow::saveCredentials(username, password);
    QMessageBox::information(this, "成功", "注册成功! 请登录。");
    emit registrationSuccess(username, password);
    this->close();

}

