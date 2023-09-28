// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "camera_attributes_data.h"
#include "camera_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CameraDataEx:
    CameraData,
    CameraAttributesData
{
    ResourceStatus status;
    std::vector<ResourceParamData> addParams;

    bool operator==(const CameraDataEx& other) const = default;
};
#define CameraDataEx_Fields \
    CameraData_Fields CameraAttributesData_Fields_Short (status)(addParams)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraDataEx)
NX_REFLECTION_INSTRUMENT(CameraDataEx, CameraDataEx_Fields)

NX_VMS_API void serialize(
    QnJsonContext* ctx, const CameraDataEx& value, QJsonValue* target);
NX_VMS_API bool deserialize(
    QnJsonContext *ctx, const QJsonValue &value, CameraDataEx *target);

} // namespace api
} // namespace vms
} // namespace nx
