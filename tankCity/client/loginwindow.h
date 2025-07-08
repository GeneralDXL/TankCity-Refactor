#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QPaintEvent>
#include <QDebug>
#include "registerwindow.h"

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

    static bool validateCredentials(const QString &username, const QString &password);
    static void saveCredentials(const QString &username, const QString &password);
    static QString accountsFilePath;

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onRegistrationSuccess(const QString &username, const QString &password);

private:
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *loginButton;
    QPushButton *registerButton;
    QPixmap backgroundImage;
};

#endif // LOGINWINDOW_H
