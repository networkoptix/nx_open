#pragma once

#include "data.h"

#include <vector>

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API AccessRightsData: Data
{
    QnUuid userId;
    std::vector<QnUuid> resourceIds;
};
#define AccessRightsData_Fields \
    (userId) \
    (resourceIds)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::AccessRightsData)
Q_DECLARE_METATYPE(nx::vms::api::AccessRightsDataList)
