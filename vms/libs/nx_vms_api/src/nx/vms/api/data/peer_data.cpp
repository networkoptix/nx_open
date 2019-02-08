#include "peer_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/fusion/serialization/lexical_enum.h>

namespace nx {
namespace vms {
namespace api {

//-------------------------------------------------------------------------------------------------
// PersistentIdData.

PersistentIdData::PersistentIdData(const QnUuid& id, const QnUuid& persistentId):
    IdData(id),
    persistentId(persistentId)
{
}

bool PersistentIdData::operator<(const PersistentIdData& other) const
{
    return id != other.id
        ? id < other.id
        : persistentId < other.persistentId;
}

//-------------------------------------------------------------------------------------------------
// PeerData.

PeerData::PeerData(): peerType(PeerType::notDefined)
{
}

PeerData::PeerData(
    const QnUuid& id,
    const QnUuid& instanceId,
    PeerType peerType,
    Qn::SerializationFormat dataFormat)
    :
    PeerData(id, instanceId, QnUuid(), peerType, dataFormat)
{
}

PeerData::PeerData(
    const PersistentIdData& peer,
    PeerType peerType,
    Qn::SerializationFormat dataFormat)
    :
    PeerData(peer.id, QnUuid(), peer.persistentId, peerType, dataFormat)
{
}

PeerData::PeerData(
    const QnUuid& id,
    const QnUuid& instanceId,
    const QnUuid& persistentId,
    PeerType peerType,
    Qn::SerializationFormat dataFormat)
    :
    PersistentIdData(id, persistentId),
    instanceId(instanceId),
    peerType(peerType),
    dataFormat(dataFormat)
{
}

bool PeerData::isClient(PeerType peerType)
{
    return peerType == PeerType::desktopClient
        || peerType == PeerType::videowallClient
        || peerType == PeerType::mobileClient
        || peerType == PeerType::oldMobileClient;
}

bool PeerData::isServer(PeerType peerType)
{
    return peerType == PeerType::server
        || peerType == PeerType::oldServer;
}

bool PeerData::isCloudServer(PeerType peerType)
{
    return peerType == PeerType::cloudServer;
}

bool PeerData::isMobileClient(PeerType peerType)
{
    return peerType == PeerType::oldMobileClient
        || peerType == PeerType::mobileClient;
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PersistentIdData)(PeerData)(PeerDataEx),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
