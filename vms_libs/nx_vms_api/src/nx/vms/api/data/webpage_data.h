#pragma once

#include "resource_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API WebPageData: ResourceData
{
    WebPageData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;
};
#define WebPageData_Fields ResourceData_Fields

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::WebPageData)
Q_DECLARE_METATYPE(nx::vms::api::WebPageDataList)
