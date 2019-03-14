#pragma once

#include "data.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CloudSystemData: Data
{
    QnUuid localSystemId;
};

#define CloudSystemData_Fields (localSystemId)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::CloudSystemData)
Q_DECLARE_METATYPE(nx::vms::api::CloudSystemDataList)
