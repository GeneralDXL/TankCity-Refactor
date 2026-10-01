#include "core/GameLoop.h"

#include <QTimer>
#include <utility>

namespace engine::core {

GameLoop::GameLoop(TickCallback onTick, QObject *context, int tickMs)
    : timer_(new QTimer(context)), onTick_(std::move(onTick)), tickMs_(tickMs)
{
    Q_ASSERT(context != nullptr);
    timer_->setInterval(tickMs_);
    QObject::connect(timer_, &QTimer::timeout, context, [this] { onTick_(); });
}

void GameLoop::start()
{
    timer_->start();
}

void GameLoop::stop()
{
    timer_->stop();
}

bool GameLoop::isRunning() const
{
    return timer_->isActive();
}

} // namespace engine::core
