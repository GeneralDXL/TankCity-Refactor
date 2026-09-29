#include "registerwindow.h"
#include "loginwindow.h"
#include "mainwindow.h"

RegisterWindow::RegisterWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("注册");
    setFixedSize(800, 700);

    backgroundImage.load("./../../assets/images/ui/login.png");
    if (backgroundImage.isNull()) {
        qDebug() << "Failed to load background image";
        setStyleSheet("background-color: #2c3e50; color: #ecf0f1;");  // 回退样式
    } else {
        setStyleSheet("color: #ecf0f1;");  // 只保留文字颜色样式
    }

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

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(20);

    QLabel *usernameLabel = new QLabel("用户名:", this);
    usernameLabel->setStyleSheet("font-size: 16px;");
    usernameEdit = new QLineEdit(this);
    usernameEdit->setFixedWidth(400);
    usernameEdit->setPlaceholderText("输入用户名");
    usernameEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QLabel *passwordLabel = new QLabel("密码:", this);
    passwordLabel->setStyleSheet("font-size: 16px;");
    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText("输入密码");
    passwordEdit->setFixedWidth(400);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("padding: 8px; border-radius: 5px; background-color: #34495e; color: #ecf0f1;");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    registerButton = createImageButton("./../../assets/images/ui/sign_up.png", BUTTON_WIDTH);

    backButton = createImageButton("./../../assets/images/ui/back.png", BUTTON_WIDTH);;

    buttonLayout->addWidget(registerButton);
    buttonLayout->addWidget(backButton);

    mainLayout->addWidget(usernameLabel,0,Qt::AlignCenter);
    mainLayout->addWidget(usernameEdit,0,Qt::AlignCenter);
    mainLayout->addWidget(passwordLabel,0,Qt::AlignCenter);
    mainLayout->addWidget(passwordEdit,0,Qt::AlignCenter);
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

void RegisterWindow::paintEvent(QPaintEvent *event)
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

