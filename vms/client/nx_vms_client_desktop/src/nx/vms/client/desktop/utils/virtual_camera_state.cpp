// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_state.h"

#include <recording/time_period_list.h>

namespace nx::vms::client::desktop {

VirtualCameraState::EnqueuedFile VirtualCameraState::currentFile() const
{
    if (currentIndex >= queue.size() || currentIndex < 0)
        return EnqueuedFile();

    return queue[currentIndex];
}

bool VirtualCameraState::isRunning() const
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

bool VirtualCameraState::isDone() const
{
    return currentIndex >= queue.size();
}

bool VirtualCameraState::isCancellable() const
{
    return isRunning() && !isDone() && (status != Consuming || currentIndex + 1 < queue.size());
}

int VirtualCameraState::progress() const
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

QnTimePeriodList VirtualCameraState::periods() const
{
    QnTimePeriodList result;

    for (const EnqueuedFile& file : queue)
        result.includeTimePeriod(file.uploadPeriod);

    return result;
}

} // namespace nx::vms::client::desktop
