#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class PeerType
{
    notDefined = -1,
    server = 0,
    desktopClient = 1,
    videowallClient = 2,
    oldMobileClient = 3,
    mobileClient = 4,
    cloudServer = 5,
    oldServer = 6, //< 2.6 or below
    count
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PeerType)

} // namespace api
} // namespace vms
} // namespace nx
