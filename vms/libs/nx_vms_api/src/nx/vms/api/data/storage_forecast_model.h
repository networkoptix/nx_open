// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include "id_data.h"

namespace nx::vms::api {

struct NX_VMS_API StorageForecastData: IdData
{
    /**%apidoc[opt] */
    std::chrono::milliseconds bitrateAnalyzePeriodMs = std::chrono::milliseconds(0);

    /**%apidoc[opt]:integer */
    double additionalSpaceB = 0;
};
#define StorageForecastData_Fields (id)(bitrateAnalyzePeriodMs)(additionalSpaceB)
QN_FUSION_DECLARE_FUNCTIONS(StorageForecastData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageForecastData, StorageForecastData_Fields)

struct NX_VMS_API StorageForecast
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc:integer Size of the recorded archive in bytes. */
    double recordedBytes = 0.0;

    /**%apidoc Recorded archive in seconds (total). */
    std::chrono::seconds recordedS = std::chrono::seconds(0);

    /**%apidoc Archive calendar duration in seconds. */
    std::chrono::seconds archiveDurationForecastS = std::chrono::seconds(0);

    /**%apidoc:integer Average bitrate (bytes / sum-of-chunk-duration-in-seconds). */
    double averageBitrate = 0.0;

    /**%apidoc:integer Average density (bytes / requested-period-in-seconds). */
    double averageDensity = 0.0;

    /**%apidoc Additional details about the recorded bytes. */
    std::map<nx::Uuid, qint64> recordedBytesPerStorage;

    void operator+=(const StorageForecast& other)
    {
        recordedBytes += other.recordedBytes;
        recordedS += other.recordedS;
        archiveDurationForecastS += other.archiveDurationForecastS;
    }
};
#define StorageForecast_Fields \
    (deviceId)(recordedBytes) \
    (recordedS)(archiveDurationForecastS)(averageBitrate)(averageDensity)(recordedBytesPerStorage)
QN_FUSION_DECLARE_FUNCTIONS(StorageForecast, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageForecast, StorageForecast_Fields)

using StorageForecastValues = std::map<nx::Uuid /*id*/, std::vector<api::StorageForecast> /*forecast*/>;

} // namespace nx::vms::api
