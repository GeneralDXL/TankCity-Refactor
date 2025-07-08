#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

class RegisterWindow : public QWidget
{
    Q_OBJECT

public:
    RegisterWindow(QWidget *parent = nullptr);

signals:
    void registrationSuccess(const QString &username, const QString &password);
    void backToLogin();

private slots:
    void onRegisterClicked();

private:
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *registerButton;
    QPushButton *backButton;
};

#endif
