// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/api/types/time_period_content_type.h>

#include "id_data.h"

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

struct DeviceFootageRequest: IdData
{
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
    size_t maxCount{INT_MAX};

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    StorageLocation storageLocation = StorageLocation::both;

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    bool includeCloudData = false;

    /**%apidoc[opt]
     * Chunk type.
     */
    TimePeriodContentType periodType = TimePeriodContentType::recording;

    // TODO: #skolesnik Implement using
    //     `std::optional<std::variant<std::vector<Region>, AnalyticsContentFilter>>`.
    //     `Region` must be defined to be serializeable to `QRegion` and `AnalyticsContentFilter`
    //     from `nx::analytics::db::Filter`.
    /**%apidoc[opt]:arrayJson filter This parameter is used for Motion and Analytics Search
     *     (if "periodType" is set to "motion" or "analytics"). Search motion or analytics event on a video
     *     according to the specified attribute values.
     *     <br/>Motion Search Format: string with a JSON array of <i>sensors</i>,
     *     each <i>sensor</i> is a JSON array of <i>rectangles</i>, each <i>rectangle</i> is:
     *     <br/>
     *     <code>{"x": <i>x</i>, "y": <i>y</i>, "width": <i>width</i>,"height": <i>height</i>}</code>
     *     <br/>All values are measured in relative portions of a video frame,
     *     <i>x</i> and <i>width</i> in range [0..43], <i>y</i> and <i>height</i> in range [0..31],
     *     zero is the left-top corner.
     *     <br/>Example of a full-frame rectangle for a single-sensor camera:
     *     <code>[[{"x":0,"y":0,"width":43,"height":31}]]</code>
     *     <br/>Example of two rectangles for a single-sensor camera:
     *     <code>[[{"x":0,"y":0,"width":5,"height":7},{"x":12,"y":10,"width":8,"height":6}]]</code>
     *     <br/>Analytics Search Format: string with a JSON object that might contain the following
     *     fields (keys):
     *     <br/>
     *     <ul>
     *     <li>"boundingBox" key represents a <i>rectangle</i>. The value is a JSON object with the
     *     same format as the Motion Search rectangle;</li>
     *     <li>"freeText" key for full-text search over analytics data attributes. The value is
     *     expected to be a string with search input;
     *     </li>
     *     </ul>
     *     <br/>Example of the JSON object:
     *     <code>{"boundingBox":{"height":0,"width":0.1,"x":0.0,"y":1.0},"freeText":"Test"}</code>
     *     <br/>
     *     <br/>ATTENTION: This field is an enquoted JSON string containing a JSON array. Example:
     *     <code>"[\\"item1\\", \\"item2\\"]"</code>
     */
    QString filter;

    bool operator==(const DeviceFootageRequest& other) const = default;
};

#define DeviceFootageRequest_Fields \
    (id)(startTimeMs)(endTimeMs)(detailLevelMs)(keepSmallChunks)(preciseBounds)(maxCount) \
    (storageLocation)(includeCloudData)(periodType)(filter)
QN_FUSION_DECLARE_FUNCTIONS(DeviceFootageRequest, (json), NX_VMS_API);

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
