#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class ConnectionType
{
    unsecure = 0,
    ssl = 1,
    tls = 2,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ConnectionType)

} // namespace api
} // namespace vms
} // namespace nx
