#pragma once

#include "camera_data.h"
#include "camera_attributes_data.h"

#include <vector>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CameraDataEx:
    CameraData,
    CameraAttributesData
{
    ResourceStatus status;
    std::vector<ResourceParamData> addParams;
};
#define CameraDataEx_Fields \
    CameraData_Fields CameraAttributesData_Fields_Short (status)(addParams)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::CameraDataEx)
Q_DECLARE_METATYPE(nx::vms::api::CameraDataExList)