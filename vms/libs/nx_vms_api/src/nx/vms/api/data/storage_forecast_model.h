// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include "id_data.h"

namespace nx::vms::api {

/**%apidoc
 * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
 *     or be `this` to refer to the current Server.
 *     %example this
 */
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

struct NX_VMS_API StorageForecastBase
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc Recorded archive in seconds (total). */
    std::chrono::seconds recordedS = std::chrono::seconds(0);
};
#define StorageForecastBase_Fields (deviceId)(recordedS)

struct NX_VMS_API StorageForecastV2: StorageForecastBase
{
    /**%apidoc:integer Forecast for the size of the recorded archive in bytes. */
    double recordedBytes = 0.0;

    /**%apidoc Forecast for the archive calendar duration in seconds. */
    std::chrono::seconds archiveDurationForecastS = std::chrono::seconds(0);

    /**%apidoc:integer Average bitrate (bytes / sum-of-chunk-duration-in-seconds). */
    double averageBitrate = 0.0;

    /**%apidoc:integer Average density (bytes / requested-period-in-seconds). */
    double averageDensity = 0.0;

    /**%apidoc Additional details about the recorded bytes. */
    std::map<nx::Uuid, qint64> recordedBytesPerStorage;
};
#define StorageForecastV2_Fields \
    StorageForecastBase_Fields(recordedBytes) \
    (archiveDurationForecastS)(averageBitrate)(averageDensity)(recordedBytesPerStorage)
QN_FUSION_DECLARE_FUNCTIONS(StorageForecastV2, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageForecastV2, StorageForecastV2_Fields)

using StorageForecastValuesV2 = std::map<nx::Uuid /*id*/, std::vector<api::StorageForecastV2> /*forecast*/>;

struct NX_VMS_API StorageForecastV4: StorageForecastBase
{
    /**%apidoc:integer Recorded archive in bytes (total). */
    double recordedB = 0.0;

    /**%apidoc:integer Forecast for the size of the recorded archive in bytes. */
    double forecastB = 0.0;

    /**%apidoc Forecast for the archive calendar duration in seconds. */
    std::chrono::seconds forecastS = std::chrono::seconds(0);

    /**%apidoc:integer Average bitrate (bytes / sum-of-chunk-duration-in-seconds). */
    double averageBps = 0.0;

    /**%apidoc:integer Average density (bytes / requested-period-in-seconds). */
    double averageDensityBps = 0.0;

    /**%apidoc:{std::map<nx::Uuid, int>} */
    std::map<nx::Uuid, double> recordedBytesPerStorage;
};
#define StorageForecastV4_Fields \
    StorageForecastBase_Fields(recordedB)(forecastB) \
    (forecastS)(averageBps)(averageDensityBps)(recordedBytesPerStorage)
QN_FUSION_DECLARE_FUNCTIONS(StorageForecastV4, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageForecastV4, StorageForecastV4_Fields)

using StorageForecastValuesV4 = std::map<nx::Uuid /*id*/, std::vector<api::StorageForecastV4> /*forecast*/>;

using StorageForecast = StorageForecastV4;
using StorageForecastValues = StorageForecastValuesV4;

} // namespace nx::vms::api
