#include "wearable_state.h"

#include <recording/time_period_list.h>

namespace nx::vms::client::desktop {

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

bool WearableState::isCancellable() const
{
    return isRunning() && !isDone() && (status != Consuming || currentIndex + 1 < queue.size());
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

QnTimePeriodList WearableState::periods() const
{
    QnTimePeriodList result;

    for (const EnqueuedFile& file : queue)
        result.includeTimePeriod(file.uploadPeriod);

    return result;
}

} // namespace nx::vms::client::desktop

