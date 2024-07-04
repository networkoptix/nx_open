// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>

#include "id_data.h"

namespace nx::vms::api {

struct NX_VMS_API RecordingStatisticsFilter: IdData
{
    /**%apidoc[opt] */
    std::chrono::milliseconds bitrateAnalyzePeriodMs = std::chrono::milliseconds(0);
};
#define RecordingStatisticsFilter_Fields (id)(bitrateAnalyzePeriodMs)
QN_FUSION_DECLARE_FUNCTIONS(RecordingStatisticsFilter, (json), NX_VMS_API)

struct NX_VMS_API RecordingStatisticsData
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc:integer Recorded archive in bytes. */
    double recordedBytes = 0.0;

    /**%apidoc Recorded archive in seconds (total). */
    std::chrono::seconds recordedS = std::chrono::seconds(0);

    /**%apidoc Archive calendar duration in seconds. */
    std::chrono::seconds archiveDurationS = std::chrono::seconds(0);

    /**%apidoc:integer Average bitrate (bytes / sum-of-chunk-duration-in-seconds). */
    double averageBitrate = 0.0;

    /**%apidoc:integer Average density ( bytes / requested-period-in-seconds). */
    double averageDensity = 0.0;

    /**%apidoc Additional details about the recorded bytes. */
    std::map<nx::Uuid, qint64> recordedBytesPerStorage;

    void operator+=(const RecordingStatisticsData& other)
    {
        recordedBytes += other.recordedBytes;
        recordedS += other.recordedS;
        archiveDurationS += other.archiveDurationS;
    }
};
#define RecordingStatisticsData_Fields (deviceId)(recordedBytes)(recordedS) \
    (archiveDurationS)(averageBitrate)(averageDensity)(recordedBytesPerStorage)
QN_FUSION_DECLARE_FUNCTIONS(RecordingStatisticsData, (json), NX_VMS_API)

using RecordingStatisticsValues = std::map<nx::Uuid /*id*/,
    std::vector<api::RecordingStatisticsData> /*statistics*/>;

} // namespace nx::vms::api
