#include "remote_archive_common.h"

#include <chrono>

namespace nx {
namespace mediaserver_core {
namespace recorder {

using namespace nx::core::resource;

QnTimePeriodList toTimePeriodList(const std::vector<RemoteArchiveChunk>& chunks)
{
    QnTimePeriodList result;
    for (const auto& chunk : chunks)
    {
        if (chunk.durationMs > 0)
            result << QnTimePeriod(chunk.startTimeMs, chunk.durationMs);
    }

    return result;
}

std::chrono::milliseconds totalDuration(
    const QnTimePeriodList& timePeriods,
    const std::chrono::milliseconds& minDuration)
{
    int64_t result = 0;
    const int minDurationMs = minDuration.count();

    for (const auto& chunk: timePeriods)
    {
        NX_ASSERT(!chunk.isInfinite());
        if (chunk.isInfinite())
            continue;

        if (chunk.durationMs < minDurationMs)
            continue;

        result += chunk.durationMs;
    }

    return std::chrono::milliseconds(result);
}

} // namespace nx
} // namespace mediaserver_core
} // namespace recorder
