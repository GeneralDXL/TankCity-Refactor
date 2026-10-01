#include "input/InputTracker.h"

namespace engine::input {

void InputTracker::keyPressed(int key)
{
    keysDown_.insert(key);
}

void InputTracker::keyReleased(int key)
{
    keysDown_.remove(key);
}

void InputTracker::setPointer(const QPoint &pos)
{
    pointer_ = pos;
}

void InputTracker::primaryButtonPressed()
{
    primaryPending_ = true;
}

InputTracker::Snapshot InputTracker::takeSnapshot()
{
    Snapshot snapshot;
    snapshot.keysDown = keysDown_;
    snapshot.pointer = pointer_;
    snapshot.primaryPressed = primaryPending_;

    // 边沿事件取走即清除：下一次快照就是 false（长按不连发）。
    primaryPending_ = false;

    return snapshot;
}

void InputTracker::reset()
{
    keysDown_.clear();
    primaryPending_ = false;
}

} // namespace engine::input
