// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/analytics/types.h>
#include <nx/vms/api/json/value_or_array.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/api/types/time_period_content_type.h>

#include "id_data.h"
#include "rect_as_string.h"

namespace nx::vms::api {

struct DevicePasswordRequest: IdData
{
    /**%apidoc
     * %example admin
     */
    QString user;

    /**%apidoc
     * %example password123
     */
    QString password;

    bool operator==(const DevicePasswordRequest& other) const = default;
};

#define DevicePasswordRequest_Fields IdData_Fields(user)(password)
QN_FUSION_DECLARE_FUNCTIONS(DevicePasswordRequest, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(DevicePasswordRequest, DevicePasswordRequest_Fields)

struct NX_VMS_API AnalyticsFilterBase
{
    NX_REFLECTION_ENUM_IN_CLASS(Option,
        none = 0x0,
        ignoreTextFilter = 0x1,
        ignoreBoundingBox = 0x2,
        ignoreTimePeriod = 0x4
    )

    Q_DECLARE_FLAGS(Options, Option)

    // TODO: #skolesnik Remove?
    /**%apidoc[opt]
     * Groups data under specified value. I.e., it allows to store multiple independent sets of
     * data in a single DB instead of having to create a separate DB instance for each unique
     * value of the storageId.
     */
    std::string storageId;

    // TODO: #mshevchenko Why 'Id' and not `Ids`? And why not `nx::analytics::ObjectTypeId`?
    std::vector<QString> objectTypeId;

    nx::Uuid objectTrackId;

    /**%apidoc[opt]
     * Set of words separated by spaces, commas, etc. The search is done across all attribute
     * values, using wildcards.
     */
    QString freeText;

    /** If true, track details (geometry data) will be selected. */
    bool needFullTrack = false;

    /**%apidoc[opt]
     * Null value treated as any engine.
     */
    nx::Uuid analyticsEngineId;

    /**%apidoc[opt] */
    Options options{};

    /**%apidoc[opt]
     * Analytic details in milliseconds.
     */
    std::chrono::milliseconds maxAnalyticsDetailsMs{};
};

struct NX_VMS_API AnalyticsFilter: AnalyticsFilterBase
{
    /**%apidoc:string Coordinates in range [0..1]. The format is `{x},{y},{width}x{height}`. */
    std::optional<RectAsString> boundingBox;

    static DeprecatedFieldNames const* getDeprecatedFieldNames();
};
#define AnalyticsFilter_Fields \
    (storageId)(objectTypeId)(objectTrackId)(boundingBox) \
    (freeText)(needFullTrack)(analyticsEngineId)(options)(maxAnalyticsDetailsMs)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsFilter, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsFilter, AnalyticsFilter_Fields)

struct DeviceFootageRequest
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Quality,
        both = 0,
        low = 1,
        high = 2)
    Q_DECLARE_FLAGS(Qualities, Quality)

    /**%apidoc:stringArray
     * Device id(s) to get Footage on. It can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`. MAC address can also be used but it is not supported
     * for certain Devices.
     */
    nx::vms::api::json::ValueOrArray<QString> id;

    /**%apidoc[opt]
     * Start time of the interval to search for chunks, in milliseconds.
     */
    std::chrono::milliseconds startTimeMs{0};

    /**%apidoc[opt]
     * End time of the interval to search for chunks, in milliseconds.
     */
    std::chrono::milliseconds endTimeMs{std::numeric_limits<qint64>::max()};

    /**%apidoc[opt]
     * Chunk detail level, in milliseconds. Time periods that are shorter than the detail level are
     * discarded. Special value "-1" indicates very large detail level so that all non-infinite chunks
     * are merged into a single chunk ("keepSmallChunks" is ignored in this case).
     */
    std::chrono::milliseconds detailLevelMs{1};

    /**%apidoc[opt]
     * If specified, standalone chunks smaller than the detail level are not removed from the
     * result.
     */
    bool keepSmallChunks{false};

    /**%apidoc[opt]
     * If specified, the chunks are precisely cropped to [startTimeMs, endTimeMs]. The default
     * behavior may produce chunks that exceed these bounds.
     */
    bool preciseBounds{false};

    /**%apidoc[opt]
     * Maximum number of chunks to return.
     */
    int maxCount{INT_MAX};

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    StorageLocation storageLocation = StorageLocation::both;

    /**%apidoc[opt] */
    Quality quality = Quality::both;

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    bool includeCloudData = false;

    /**%apidoc[opt] Chunk type. */
    TimePeriodContentType periodType = TimePeriodContentType::recording;

    // TODO: Combine motion and anayltics below to std::optional<std::variant> when query parameter
    // with oneOf will be correctly processed by Swagger UI.

    /**%apidoc:stringArray
     * Coordinates in range [0:1]. The format is `{x},{y},{width}x{height}` per each item. Must be
     * used with `periodType` equals to `motion` only.
     * %// Appeared starting from /rest/v4/devices/{id}/footage.
     */
    std::optional<json::ValueOrArray<RectAsString>> motion;

    /**%apidoc
     * Must be used with `periodType` equals to `analytics` only.
     * %// Appeared starting from /rest/v4/devices/{id}/footage.
     */
    std::optional<AnalyticsFilter> analytics;
};
#define DeviceFootageRequest_Fields \
    (id)(startTimeMs)(endTimeMs)(detailLevelMs)(keepSmallChunks)(preciseBounds)(maxCount) \
    (storageLocation)(quality)(includeCloudData)(periodType)(motion)(analytics)
QN_FUSION_DECLARE_FUNCTIONS(DeviceFootageRequest, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(DeviceFootageRequest, DeviceFootageRequest_Fields)

struct DeviceDiagnosis
{
    /**%apidoc Device id. */
    nx::Uuid id;

    /**%apidoc Device status. */
    ResourceStatus status = ResourceStatus::undefined;

    /**%apidoc Last Device initialization result. */
    QString init;

    /**%apidoc Device video stream state. */
    QString stream;

    /**%apidoc Last Device media error if any. */
    QString media;
};
#define DeviceDiagnosis_Fields (id)(status)(init)(stream)(media)
QN_FUSION_DECLARE_FUNCTIONS(DeviceDiagnosis, (json), NX_VMS_API);

} // namespace nx::vms::api
