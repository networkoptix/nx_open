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

OverlappedTimePeriods toOverlappedTimePeriods(
    const OverlappedRemoteChunks& overlappedChunks)
{
    OverlappedTimePeriods result;

    for (const auto& entry: overlappedChunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunks = entry.second;

        result[overlappedId] = toTimePeriodList(chunks);
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

std::chrono::milliseconds totalDuration(
    const OverlappedTimePeriods& overlappedTimeline,
    const std::chrono::milliseconds& minDuration)
{
    std::vector<QnTimePeriodList> periodsToMerge;
    for (const auto& entry: overlappedTimeline)
        periodsToMerge.push_back(entry.second);

    const auto mergedPeriods = QnTimePeriodList::mergeTimePeriods(periodsToMerge);
    return totalDuration(mergedPeriods, minDuration);
}

QnTimePeriodList mergeOverlappedChunks(const nx::core::resource::OverlappedRemoteChunks& chunks)
{
    std::vector<QnTimePeriodList> periodsToMerge;
    for (const auto& entry: chunks)
        periodsToMerge.push_back(toTimePeriodList(entry.second));

    return QnTimePeriodList::mergeTimePeriods(periodsToMerge);

}

} // namespace nx
} // namespace mediaserver_core
} // namespace recorder
