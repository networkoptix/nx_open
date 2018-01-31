#include "wearable_state.h"

namespace nx {
namespace client {
namespace desktop {

WearableState::EnqueuedFile WearableState::currentFile() const
{
    if (currentIndex >= queue.size() || currentIndex < 0)
        return EnqueuedFile();

    return queue[currentIndex];
}

bool WearableState::isRunning() const
{
    switch (status)
    {
    case Locked:
    case Uploading:
    case Consuming:
        return true;
    default:
        return false;
    }
}

bool WearableState::isDone() const
{
    return currentIndex >= queue.size();
}

int WearableState::progress() const
{
    if (queue.isEmpty())
        return 0;

    int filePercent = 0;
    switch (status)
    {
    case Uploading:
        if (currentUpload.size == 0)
            filePercent = 0;
        else
            filePercent = 90 * currentUpload.uploaded / currentUpload.size;
        break;
    case Consuming:
        filePercent = 90 + 10 * consumeProgress / 100;
        break;
    default:
        break;
    }

    return (100 * currentIndex + filePercent) / queue.size();
}

} // namespace desktop
} // namespace client
} // namespace nx

