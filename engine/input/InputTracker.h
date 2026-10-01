#ifndef ENGINE_INPUT_INPUTTRACKER_H
#define ENGINE_INPUT_INPUTTRACKER_H

#include <QPoint>
#include <QSet>

namespace engine::input {

/**
 * 输入状态容器：记录「哪些键按着、指针在哪、主键是否刚被按下」。
 *
 * ## 它刻意不含玩法语义
 * 不做「移动向量 / 瞄准向量」的归一化或组合，也不知道哪个键是前进。
 * 那些属于 M3 的双摇杆改造，而且**会改变与服务器之间的协议**（目前上行是
 * `keys: {w,a,s,d}` 四个布尔），所以本里程碑只做容器：
 * 把客户端已有的「按键集合 + 指针位置」这两份状态收进一处。
 * 「哪个键算什么」留在客户端（`client/gamewindow.cpp` 里那张键位表）。
 *
 * ## 必须保持不变的行为：主键是边沿触发的
 * 旧代码在每次组包后执行 `pressedKeys.remove(Qt::LeftButton)`
 * （搬迁前的 `gamewindow.cpp:598`），效果是**一次点击只发一次 shoot=true**，
 * 长按不连发 —— 这是基线行为清单第 8 项钉住的既有行为。
 * 这里等价地表达为：`primaryButtonPressed()` 置一个待发标记，
 * `takeSnapshot()` **取走即清除**。
 */
class InputTracker
{
public:
    /// 一次输入快照。取值是原始的键值与指针位置，不含任何玩法解释。
    struct Snapshot
    {
        QSet<int> keysDown;            ///< 当前按下的键（Qt::Key 值）
        QPoint pointer;                ///< 指针位置
        bool primaryPressed = false;   ///< 主键自上次快照以来是否被按下（边沿触发）
    };

    /// 某个键被按下。
    void keyPressed(int key);
    /// 某个键被松开（对任何键都应当调用，否则集合会只增不减）。
    void keyReleased(int key);
    /// 指针移动。
    void setPointer(const QPoint &pos);
    /// 记录一次主键按下（边沿事件，不是「按着不放」的状态）。
    void primaryButtonPressed();

    /// 取走当前状态；`primaryPressed` 取走后即清除。
    Snapshot takeSnapshot();

    bool isKeyDown(int key) const { return keysDown_.contains(key); }

    /**
     * 清空「按下的键」与「待发的主键事件」，**指针位置保留**。
     * 等价于搬迁前的 `pressedKeys.clear()`（开局时清掉残留按键）。
     */
    void reset();

private:
    QSet<int> keysDown_;
    QPoint pointer_;
    bool primaryPending_ = false;
};

} // namespace engine::input

#endif // ENGINE_INPUT_INPUTTRACKER_H
