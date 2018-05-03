#pragma once

#include <recording/time_period_list.h>
#include <core/resource/abstract_remote_archive_manager.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

QnTimePeriodList toTimePeriodList(
    const std::vector<nx::core::resource::RemoteArchiveChunk>& chunks);

nx::core::resource::OverlappedTimePeriods toOverlappedTimePeriods(
    const nx::core::resource::OverlappedRemoteChunks& overlappedChunks);

std::chrono::milliseconds totalDuration(
    const QnTimePeriodList& timePeriods,
    const std::chrono::milliseconds& minDuration);

std::chrono::milliseconds totalDuration(
    const nx::core::resource::OverlappedTimePeriods& overlappedTimeline,
    const std::chrono::milliseconds& minDuration);

QnTimePeriodList mergeOverlappedChunks(
    const nx::core::resource::OverlappedRemoteChunks& chunks);

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
