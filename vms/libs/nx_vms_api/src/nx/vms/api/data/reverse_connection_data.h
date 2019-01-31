#pragma once

#include "data.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

/** Request to open count proxy connections to @var target server. */
struct NX_VMS_API ReverseConnectionData: Data
{
    QnUuid targetServer;
    int socketCount = 0;
};
#define ReverseConnectionData_Fields (targetServer)(socketCount)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ReverseConnectionData)
