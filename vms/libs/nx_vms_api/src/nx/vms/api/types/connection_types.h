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
    oldServer = 6 //< 2.6 or below
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PeerType)

enum class RuntimeFlag
{
    /** Sync transactions with cloud. */
    masterCloudSync = 1 << 0,
    noStorages = 1 << 1
};
Q_DECLARE_FLAGS(RuntimeFlags, RuntimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(RuntimeFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RuntimeFlag)

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::RuntimeFlag, (metatype)(lexical)(debug), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::RuntimeFlags, (metatype)(lexical)(debug), NX_VMS_API)
