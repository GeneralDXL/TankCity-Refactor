#include "scoreboard.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <QWidget>
#include <QLabel>


ScoreBoard::ScoreBoard(QWidget *parent) : QWidget(parent)
{
    setFixedSize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *titleLabel = new QLabel("排行榜", this);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #3498db;");
    titleLabel->setAlignment(Qt::AlignCenter);

    scoreTable = new QTableWidget(this);
    scoreTable->setColumnCount(3);
    scoreTable->setHorizontalHeaderLabels({"排名", "玩家", "分数"});
    scoreTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    scoreTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scoreTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    scoreTable->setStyleSheet("QTableWidget { background-color: #34495e; color: #ecf0f1; gridline-color: #2c3e50; }"
                              "QHeaderView::section { background-color: #2c3e50; color: #ecf0f1; }");

    backButton = new QPushButton("返回主菜单", this);
    backButton->setStyleSheet("QPushButton { background-color: #e67e22; color: white; border-radius: 5px; padding: 10px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #d35400; }");
    backButton->setFixedSize(150, 40);

    layout->addWidget(titleLabel);
    layout->addWidget(scoreTable);
    layout->addWidget(backButton, 0, Qt::AlignCenter);

    connect(backButton, &QPushButton::clicked, this, &ScoreBoard::backToMenu);
}

void ScoreBoard::loadScores()
{
    scores.clear();


    QFile file("data/scores.txt");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split(":");
            if (parts.size() == 2) {
                QString username = parts[0];
                int score = parts[1].toInt();
                scores.append(qMakePair(username, score));
            }
        }
        file.close();
    }


    std::sort(scores.begin(), scores.end(), [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
        return a.second > b.second;
    });

    int rowCount = qMin(10, scores.size());
    scoreTable->setRowCount(rowCount);

    for (int i = 0; i < rowCount; i++) {
        QTableWidgetItem *rankItem = new QTableWidgetItem(QString::number(i + 1));
        QTableWidgetItem *nameItem = new QTableWidgetItem(scores[i].first);
        QTableWidgetItem *scoreItem = new QTableWidgetItem(QString::number(scores[i].second));

        rankItem->setTextAlignment(Qt::AlignCenter);
        nameItem->setTextAlignment(Qt::AlignCenter);
        scoreItem->setTextAlignment(Qt::AlignCenter);

        // Set row color based on rank
        QString rowColor;
        if (i == 0) {
            rowColor = "#FFD700"; // Gold
        } else if (i == 1) {
            rowColor = "#C0C0C0"; // Silver
        } else if (i == 2) {
            rowColor = "#CD7F32"; // Bronze
        } else {
            rowColor = "#ecf0f1"; // Default
        }

        rankItem->setBackground(QColor(rowColor));
        nameItem->setBackground(QColor(rowColor));
        scoreItem->setBackground(QColor(rowColor));

        rankItem->setForeground(Qt::black);
        nameItem->setForeground(Qt::black);
        scoreItem->setForeground(Qt::black);

        scoreTable->setItem(i, 0, rankItem);
        scoreTable->setItem(i, 1, nameItem);
        scoreTable->setItem(i, 2, scoreItem);
    }

    scoreTable->resizeColumnsToContents();
}
