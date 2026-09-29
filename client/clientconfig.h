#ifndef CLIENTCONFIG_H
#define CLIENTCONFIG_H

/**
 * @file  clientconfig.h
 * @brief 客户端的配置入口：读 assets/config 与 assets/levels，不自己留一份副本。
 *
 * 为什么客户端也要读配置：数值必须与判伤的服务端用同一套口径。
 * 典型反例——血条按 `health * 50 / 100` 画，而 M2 之后满血是 10，
 * 于是玩家满血只画出 5px 的血条。要避免这类"两边各算各的"，
 * 客户端就只能有一处数值来源，那就是 assets/config。
 *
 * 配置读不到时**不抛异常**：登录、菜单这些界面不依赖配置，不该被一个坏文件拖死。
 * 各取值函数退化为零值，由使用处决定如何降级。
 */

#include <QString>
#include <QVector>

#include "config/ConfigTypes.h"
#include "config/TankStats.h"

namespace client {

/// `<exe 目录>/../../assets` —— 与 server/game.cpp 的 assetsRoot() 同口径。
QString assetsRoot();

/**
 * assets/config 下的六份 JSON，首次调用时加载并缓存。
 *
 * @return 缓存对象；加载失败时返回 nullptr（并 qCritical 一次，不重复刷屏）。
 */
const tankcity::config::Config *config();

/** 玩家坦克的原型数值（entities.json 里的 tanks.player）；拿不到时为零值。 */
tankcity::config::TankStats playerStats();

/**
 * 第 @p difficulty 档敌人的数值。
 *
 * @p difficulty 是客户端的选档下标（0/1/2），按 difficulty.json 的书写顺序取档 ——
 * 与服务端 `Game::resolveCurrentEnemyStats()` 同一口径。
 */
tankcity::config::TankStats enemyStats(int difficulty);

/**
 * assets/levels 下的关卡列表（按序号升序），用于填充地图下拉框。
 *
 * 这是「新增关卡不改 C++」的落点：把 level_11.json 放进目录，菜单里就会多一项。
 */
QVector<tankcity::config::LevelEntry> levels();

} // namespace client

#endif // CLIENTCONFIG_H
