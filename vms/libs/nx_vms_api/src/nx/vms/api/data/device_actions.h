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
    QString user;
    QString password;

    bool operator==(const DevicePasswordRequest& other) const = default;
};

#define DevicePasswordRequest_Fields IdData_Fields (user)(password)
QN_FUSION_DECLARE_FUNCTIONS(DevicePasswordRequest, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(DevicePasswordRequest, DevicePasswordRequest_Fields)

struct DeviceFootageRequest: IdData
{
    /**%apidoc:integer */
    std::chrono::milliseconds startTimeMs{0};
    /**%apidoc:integer */
    std::chrono::milliseconds endTimeMs{std::numeric_limits<qint64>::max()};
    /**%apidoc:integer */
    std::chrono::milliseconds detailLevelMs{1};
    bool keepSmallChunks{false};
    bool preciseBounds{false};
    size_t maxCount{INT_MAX};

    /**%apidoc[opt]
     * %// Appeared starting from /rest/v2/devices/{id}/footage.
     */
    StorageLocation storageLocation = StorageLocation::both;

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
    QnUuid id;
    ResourceStatus status = ResourceStatus::undefined;
    QString init;
    QString media;
};
#define DeviceDiagnosis_Fields (id)(status)(init)(media)
QN_FUSION_DECLARE_FUNCTIONS(DeviceDiagnosis, (json), NX_VMS_API);

} // namespace nx::vms::api
