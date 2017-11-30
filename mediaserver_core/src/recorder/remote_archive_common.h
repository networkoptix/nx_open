#pragma once

#include <recording/time_period_list.h>
#include <core/resource/abstract_remote_archive_manager.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

QnTimePeriodList toTimePeriodList(
    const std::vector<nx::core::resource::RemoteArchiveChunk>& chunks);

std::chrono::milliseconds totalDuration(
    const QnTimePeriodList& timePeriods,
    const std::chrono::milliseconds& minDuration);

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
