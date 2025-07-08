#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *registerButton;
    QPushButton *backButton;
    QPixmap backgroundImage;
};

#endif
