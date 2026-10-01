#include "scoreboard.h"
#include "asset/AssetPaths.h"
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <QLabel>
#include <QGraphicsEffect>


ScoreBoard::ScoreBoard(QWidget *parent) : QWidget(parent)
{
    setFixedSize(1200, 900);
    setStyleSheet("background: transparent;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(50,50,50,50);

    QLabel *titleLabel = new QLabel(this);
    QPixmap scoreboardPixmap(engine::asset::assetPath("images/ui/row.png")); // 使用排行榜图片
    if(!scoreboardPixmap.isNull()) {
        titleLabel->setPixmap(scoreboardPixmap.scaledToWidth(400, Qt::SmoothTransformation));
        titleLabel->setAlignment(Qt::AlignCenter);
    } else {
        titleLabel->setText("排行榜");
        titleLabel->setStyleSheet(
            "font-size: 36px;"
            "font-weight: bold;"
            "color: #e74c3c;"
            "font-family: '华文行楷';"
            );
    }

    scoreTable = new QTableWidget(this);
    scoreTable->setColumnCount(3);
    scoreTable->setHorizontalHeaderLabels({"排名", "玩家", "分数"});
    scoreTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    scoreTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    scoreTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    scoreTable->setStyleSheet(
        "QTableWidget {"
        "   background-color: rgba(52, 73, 94, 0.7);"  // 半透明背景
        "   color: #ecf0f1;"
        "   border: 2px solid #3498db;"
        "   border-radius: 10px;"
        "   font-family: '华文行楷';"
        "}"
        "QHeaderView::section {"
        "   background-color: #2c3e50;"
        "   color: #ecf0f1;"
        "   font-family: '华文行楷';"
        "}"
        );

    backButton = new QPushButton( this);

    QPixmap returnPixmap(engine::asset::assetPath("images/ui/return.png"));
    if (!returnPixmap.isNull()) {
        backButton->setIcon(QIcon(returnPixmap.scaledToWidth(200, Qt::SmoothTransformation)));
        backButton->setIconSize(returnPixmap.scaledToWidth(200, Qt::SmoothTransformation).size());
    }
    backButton->setFixedSize(200, returnPixmap.height() * 200 / returnPixmap.width());
    backButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "QPushButton:hover {"
        "   opacity: 0.7;"
        "}"
        );

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(backButton);
    effect->setOpacity(1.0);
    backButton->setGraphicsEffect(effect);

    backButton->installEventFilter(new HoverEventFilter(effect, backButton));

    layout->addWidget(titleLabel);
    layout->addWidget(scoreTable);
    layout->addWidget(backButton, 0, Qt::AlignCenter);

    connect(backButton, &QPushButton::clicked, this, &ScoreBoard::backToMenu);
}

void ScoreBoard::loadScores()
{
    scores.clear();


    QFile file(engine::asset::dataPath("scores.txt"));
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
