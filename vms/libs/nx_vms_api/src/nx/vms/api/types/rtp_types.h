#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class RtpTransportType
{
    automatic,
    tcp,
    udp,
    multicast,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RtpTransportType)

} // namespace api
} // namespace vms
} // namespace nx
