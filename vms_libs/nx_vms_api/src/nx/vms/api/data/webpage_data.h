#pragma once

#include "resource_data.h"

namespace nx {
namespace vms {
namespace api {

struct WebPageData: ResourceData
{
};
#define WebPageData_Fields ResourceData_Fields

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::WebPageData)
