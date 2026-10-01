#ifndef ENGINE_CORE_GAMELOOP_H
#define ENGINE_CORE_GAMELOOP_H

#include <QObject>
#include <functional>

class QTimer;

namespace engine::core {

/// 逻辑帧长（毫秒）—— 「一帧」在全仓库唯一定义于此。
///
/// 帧数是逻辑单位（见 shared/config 的 `*Ticks`），所以服务端与客户端必须同值：
/// 客户端目前还写着自己的 16（M1.1 不改客户端循环，见决议 D2），最终要收敛到这里。
inline constexpr int kTickMs = 16;

/**
 * 固定频率逻辑循环。
 *
 * **与 M1.1 之前逐字节等价**：内部就是一个 `QTimer(kTickMs)`，每次超时调用一次回调，
 * 没有累加器、没有插值 —— 超时即一帧。这是有意的取舍（决议 D2/D3）：
 * 累加器要等 M6 表现层有了独立渲染循环、真的需要「补帧」时再引入；
 * 现在引入会改变行为（落后时一次补多帧），而 M1.1 的验收口径是可玩性不变。
 *
 * 它顺带收拢两件事：
 *  - `16` 这个魔数 → `kTickMs`；
 *  - 「new timer / connect / start / stop / disconnect」这套仪式 → 三个方法。
 *
 * ## 两个不能改的实现约束
 *
 * 1. **内部 timer 必须 parent 到 `context`**：`Game` 会被 `moveToThread()` 搬到工作线程
 *    （server.cpp:285/304），Qt 只会把**父子关系**里的子对象一起迁移。若 timer 不设 parent，
 *    它会留在原线程、循环直接失效。
 * 2. **stop() 只停表，不再 disconnect**：旧代码在 5 处写成
 *    `gameTimer->stop(); disconnect(gameTimer, nullptr, this, nullptr);`，
 *    而这 5 处**全部紧跟在 `isEnd = true;` 之后**，`Game::gameLoop()` 开头就是
 *    `if (isEnd) return;` —— 也就是说 disconnect 是可证明多余的，`stop()` 语义等价。
 *    （顺带避免了「disconnect 掉连接后就再也 start 不起来」这个隐患。）
 */
class GameLoop
{
public:
    /// 每帧回调。
    using TickCallback = std::function<void()>;

    /**
     * @param onTick  每帧回调（在 `context` 所属线程执行）
     * @param context 拥有内部 timer 的 QObject，决定线程亲和性与生命周期；不可为空
     * @param tickMs  帧长，默认 `kTickMs`
     */
    GameLoop(TickCallback onTick, QObject *context, int tickMs = kTickMs);

    GameLoop(const GameLoop &) = delete;
    GameLoop &operator=(const GameLoop &) = delete;

    void start();
    void stop();
    bool isRunning() const;

    int tickMs() const { return tickMs_; }

private:
    QTimer *timer_;        ///< 归属 `context`（父对象负责释放，并带它一起迁移线程）
    TickCallback onTick_;
    int tickMs_;
};

} // namespace engine::core

#endif // ENGINE_CORE_GAMELOOP_H
