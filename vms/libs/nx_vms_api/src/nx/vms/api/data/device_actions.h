// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QJsonObject>

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

#define DevicePasswordRequest_Fields IdData_Fields (user)(password)
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
     * Chunk type.
     */
    TimePeriodContentType periodType = TimePeriodContentType::recording;

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    bool includeCloudData = false;

    bool operator==(const DeviceFootageRequest& other) const = default;
};

#define DeviceFootageRequest_Fields \
    (id)(startTimeMs)(endTimeMs)(detailLevelMs)(keepSmallChunks)(preciseBounds)(maxCount) \
    (storageLocation)(periodType)(includeCloudData)
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

struct DeviceResourceData
{
    /**%apidoc Device id. */
    nx::Uuid id;

    /**%apidoc Device resource data. */
    QJsonObject data;
};
#define DeviceResourceData_Fields (id)(data)
QN_FUSION_DECLARE_FUNCTIONS(DeviceResourceData, (json), NX_VMS_API);

} // namespace nx::vms::api
