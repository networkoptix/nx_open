#include "wearable_state.h"

namespace nx {
namespace client {
namespace desktop {

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

int WearableState::progress() const
{
    switch (status)
    {
    case Unlocked:
    case LockedByOtherClient:
    case Locked:
        return 0;
    case Uploading:
        return 90 * currentUpload.uploaded / currentUpload.size;
    case Consuming:
        return 90 + 10 * consumeProgress / 100;
    default:
        return 0;
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

