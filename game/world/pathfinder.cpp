#include "pathfinder.h"

#include "world.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

// M3：本文件是 Enemy::calculatePath() 的搬迁 + 清理，**算法本身未改**。
//   - 所有权：原来用 new/delete 手工管理节点，现在全部交给 unique_ptr（见 allNodes）；
//   - 依赖：原来吃 Map*，现在吃 World（网格与物块的归属地）；
//   - 边界：原来写死 30 / 22.5，现在一律取自 world.getGridWidth()/getGridHeight()。

namespace {

/// A* 节点。所有权归 allNodes（unique_ptr），优先队列里只放裸指针。
struct Node
{
    QPoint cell;
    int g = 0;   ///< 起点到本格的实际代价
    int h = 0;   ///< 到目标的预估代价（曼哈顿，与旧实现一致）
    Node *parent = nullptr;

    int f() const { return g + h; }
};

struct CellLess
{
    bool operator()(const QPoint &a, const QPoint &b) const
    {
        if (a.x() != b.x())
            return a.x() < b.x();
        return a.y() < b.y();
    }
};

/// 八方向偏移。**顺序与旧实现一致**：从「上」开始顺时针，四正后四斜。
/// 顺序会影响同代价路径选中哪条，所以它不是随便排的。
const QPoint kDirections[] = {
    QPoint(0, -1),  // 上
    QPoint(1, 0),   // 右
    QPoint(0, 1),   // 下
    QPoint(-1, 0),  // 左
    QPoint(-1, -1), // 左上
    QPoint(1, -1),  // 右上
    QPoint(1, 1),   // 右下
    QPoint(-1, 1)   // 左下
};

constexpr int kStraightCost = 10;
constexpr int kDiagonalCost = 14;

/// 格点唯一键（与旧实现同一套编码，仅用于哈希表查重）。
int cellKey(const QPoint &cell)
{
    return cell.x() * 10000 + cell.y();
}

/// 视线可达就跳过中间点 —— 把 A* 的锯齿路径拉直。
/// 判据是 `World::isLineWalkable`（Bresenham 走线，逐格判可通行）。
QVector<QPoint> simplifyPath(const World &world, const QVector<QPoint> &path)
{
    if (path.size() <= 2)
        return path;

    QVector<QPoint> simplified;
    simplified.append(path.first());

    int lastValidIndex = 0;
    for (int i = 1; i < path.size() - 1; ++i) {
        if (!world.isLineWalkable(simplified.last(), path[i + 1])) {
            // 拉不直：补一个中间点（若跨了不止一格），再把当前点收进来
            if (i - lastValidIndex > 1) {
                const QPoint midPoint = (simplified.last() + path[i]) / 2;
                if (world.isCellWalkable(midPoint.x(), midPoint.y()))
                    simplified.append(midPoint);
            }
            simplified.append(path[i]);
            lastValidIndex = i;
        }
    }
    simplified.append(path.last());

    return simplified;
}

} // namespace

QPoint PathFinder::snapToWalkable(const World &world, const QPoint &cell, int maxRadius)
{
    if (world.isCellWalkable(cell.x(), cell.y()))
        return cell;

    // 由内向外逐圈扫，取**第一个**可通行格 —— 即切比雪夫距离最近的候选。
    //
    // 旧实现只 `break` 内层循环，于是每圈的候选会被后续候选覆盖，
    // 最终返回的是"最后一个"而不是"最近的"。M3 步 4a 曾照搬它以保持行为不变；
    // 4c 改掉，因为实测证明它会放大"无敌点"问题：玩家贴着墙站时其所在格不可通行，
    // 终点被吸附到一个远处的格，敌人走到那儿就以为到了（详见 enemy.h 的说明）。
    for (int r = 1; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dy = -r; dy <= r; ++dy) {
                const QPoint candidate(cell.x() + dx, cell.y() + dy);
                if (world.isCellWalkable(candidate.x(), candidate.y()))
                    return candidate;
            }
        }
    }
    return cell;
}

QVector<QPoint> PathFinder::findPath(const World &world,
                                     const QPoint &startCell,
                                     const QPoint &targetCell)
{
    QVector<QPoint> path;

    const QPoint start = snapToWalkable(world, startCell);
    const QPoint target = snapToWalkable(world, targetCell);

    // 已经在目标格上：给一格，让调用方朝它走（旧实现如此）
    if (start == target) {
        path.append(target);
        return path;
    }

    // 同 f 时先进 h 小的（与旧实现一致）
    auto cmp = [](const Node *a, const Node *b) {
        if (a->f() == b->f())
            return a->h > b->h;
        return a->f() > b->f();
    };
    std::priority_queue<Node *, std::vector<Node *>, decltype(cmp)> openSet(cmp);
    std::set<QPoint, CellLess> closedSet;
    std::unordered_map<int, std::unique_ptr<Node>> allNodes;   // 唯一所有权：不会漏 delete

    auto *startNode = new Node{start, 0,
                               qAbs(start.x() - target.x()) + qAbs(start.y() - target.y()), nullptr};
    allNodes.emplace(cellKey(start), std::unique_ptr<Node>(startNode));
    openSet.push(startNode);

    Node *targetNode = nullptr;

    while (!openSet.empty()) {
        Node *currentNode = openSet.top();
        openSet.pop();

        // 同一个格可能被多次入队（g 变小时会重新入队）。已经展开过的跳过 ——
        // 旧实现没有这一条，会重复展开同一个格，只浪费算力、不改结果。
        if (closedSet.find(currentNode->cell) != closedSet.end())
            continue;

        if (currentNode->cell == target) {
            targetNode = currentNode;
            break;
        }

        closedSet.insert(currentNode->cell);

        for (const QPoint &dir : kDirections) {
            const QPoint neighbor = currentNode->cell + dir;

            // 边界取自地图自身的格数（旧实现这里写着 30 与 22.5）
            if (neighbor.x() < 0 || neighbor.y() < 0 ||
                neighbor.x() >= world.getGridWidth() || neighbor.y() >= world.getGridHeight()) {
                continue;
            }
            if (closedSet.find(neighbor) != closedSet.end())
                continue;

            // **只判目标格**：斜向邻居允许穿过墙角（M3 决议 D3）
            if (!world.isCellWalkable(neighbor.x(), neighbor.y()))
                continue;

            const int moveCost = (qAbs(dir.x()) + qAbs(dir.y()) == 2) ? kDiagonalCost : kStraightCost;
            const int newG = currentNode->g + moveCost;
            const int newH = qAbs(neighbor.x() - target.x()) + qAbs(neighbor.y() - target.y());

            auto it = allNodes.find(cellKey(neighbor));
            if (it == allNodes.end()) {
                auto *node = new Node{neighbor, newG, newH, currentNode};
                allNodes.emplace(cellKey(neighbor), std::unique_ptr<Node>(node));
                openSet.push(node);
            } else if (newG < it->second->g) {
                Node *node = it->second.get();
                node->g = newG;
                node->h = newH;
                node->parent = currentNode;
                openSet.push(node);   // 重新入队以反映新的 f
            }
        }
    }

    if (targetNode == nullptr)
        return path;   // 无路可走

    // 回溯并反转（旧实现用 prepend，这里 reverse 一次，结果相同）
    for (Node *node = targetNode; node != nullptr; node = node->parent)
        path.append(node->cell);
    std::reverse(path.begin(), path.end());

    // 去掉起点：调用方已经在那一格里了
    if (path.size() > 1)
        path.removeFirst();

    return simplifyPath(world, path);
}
