// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "runtime_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

bool RuntimeData::operator==(const RuntimeData& other) const
{
    return version == other.version
        && peer == other.peer
        && platform == other.platform
        && publicIP == other.publicIP
        && videoWallInstanceGuid == other.videoWallInstanceGuid
        && videoWallControlSession == other.videoWallControlSession
        && prematureLicenseExperationDate == other.prematureLicenseExperationDate
        && prematureVideoWallLicenseExpirationDate == other.prematureVideoWallLicenseExpirationDate
        && hardwareIds == other.hardwareIds
        && updateStarted == other.updateStarted
        && nx1mac == other.nx1mac
        && nx1serial == other.nx1serial
        && userId == other.userId
        && flags == other.flags
        && activeAnalyticsEngines == other.activeAnalyticsEngines
        && tierGracePeriodExpirationDateMs == other.tierGracePeriodExpirationDateMs
        ;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RuntimeData, (ubjson)(json)(xml), RuntimeData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
