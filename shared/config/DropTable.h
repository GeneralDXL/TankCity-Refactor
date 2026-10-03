#ifndef TANKCITY_DROP_TABLE_H
#define TANKCITY_DROP_TABLE_H

#include "ConfigTypes.h"

#include <QVector>

namespace tankcity::config {

/**
 * 按权重从掉落表里抽一项（M4 步 5，"① 地形即资源"）。
 *
 * @param drops 掉落表（`blocks.json` 的 `drops`，加载期已校验 item 存在、weight 为正）
 * @param roll01 均匀随机数，取值 **[0, 1)**（由调用方提供，便于测试注入）
 * @return 抽中的那一项；空表或权重非正时返回 `nullptr`
 *
 * 为什么做成**纯函数**放在 shared：抽签规则要能单测（给定 roll 必得确定结果），
 * 而调用方（服务端）只负责提供随机数与"生成在哪"。这样"掉落分布"这件事
 * 不依赖任何关卡、网络或随机源，权重改错会立刻被用例抓住。
 */
inline const DropDef *pickDrop(const QVector<DropDef> &drops, double roll01)
{
    int total = 0;
    for (const DropDef &drop : drops) {
        if (drop.weight > 0)
            total += drop.weight;
    }
    if (total <= 0)
        return nullptr;

    double remaining = roll01 * total;
    for (const DropDef &drop : drops) {
        if (drop.weight <= 0)
            continue;
        remaining -= drop.weight;
        if (remaining < 0.0)
            return &drop;
    }
    // 浮点边界兜底（roll01 恰好被夹在 [0,1) 之外时）
    return &drops.last();
}

} // namespace tankcity::config

#endif // TANKCITY_DROP_TABLE_H
