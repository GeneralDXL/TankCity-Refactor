#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVector>
#include <QPair>

class ScoreBoard : public QWidget
{
    Q_OBJECT

public:
    explicit ScoreBoard(QWidget *parent = nullptr);
    void loadScores();

signals:
    void backToMenu();

private:
    QTableWidget *scoreTable;
    QPushButton *backButton;

    QVector<QPair<QString, int>> scores;
};

#endif
