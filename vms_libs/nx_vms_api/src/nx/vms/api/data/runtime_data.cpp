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
        && box == other.box
        && publicIP == other.publicIP
        && videoWallInstanceGuid == other.videoWallInstanceGuid
        && videoWallControlSession == other.videoWallControlSession
        && prematureLicenseExperationDate == other.prematureLicenseExperationDate
        && hardwareIds == other.hardwareIds
        && updateStarted == other.updateStarted
        && nx1mac == other.nx1mac
        && nx1serial == other.nx1serial
        && userId == other.userId
        && flags == other.flags;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (RuntimeData),
    (ubjson)(json)(xml),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
