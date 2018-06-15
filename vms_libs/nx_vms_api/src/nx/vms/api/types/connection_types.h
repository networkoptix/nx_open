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

enum class TimeFlag
{
    none = 0,
    peerIsNotEdgeServer = 0x1,
    peerHasMonotonicClock = 0x2,
    peerTimeSetByUser = 0x4,
    peerTimeSynchronizedWithInternetServer = 0x8,
    peerIsServer = 0x1000
};
Q_DECLARE_FLAGS(TimeFlags, TimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(TimeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TimeFlag)

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::TimeFlags, (metatype)(lexical)(debug), NX_VMS_API)
