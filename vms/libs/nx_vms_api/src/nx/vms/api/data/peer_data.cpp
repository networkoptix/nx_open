// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "peer_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/connection_types.h>

namespace nx {
namespace vms {
namespace api {

//-------------------------------------------------------------------------------------------------
// PersistentIdData.

PersistentIdData::PersistentIdData(const nx::Uuid& id, const nx::Uuid& persistentId):
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
    const nx::Uuid& id,
    const nx::Uuid& instanceId,
    PeerType peerType,
    Qn::SerializationFormat dataFormat)
    :
    PeerData(id, instanceId, nx::Uuid(), peerType, dataFormat)
{
}

PeerData::PeerData(
    const PersistentIdData& peer,
    PeerType peerType,
    Qn::SerializationFormat dataFormat)
    :
    PeerData(peer.id, nx::Uuid(), peer.persistentId, peerType, dataFormat)
{
}

PeerData::PeerData(
    const nx::Uuid& id,
    const nx::Uuid& instanceId,
    const nx::Uuid& persistentId,
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentIdData, (ubjson)(xml)(json)(sql_record)(csv_record), PersistentIdData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PeerData, (ubjson)(xml)(json)(sql_record)(csv_record), PeerData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PeerDataEx, (ubjson)(xml)(json)(sql_record)(csv_record), PeerDataEx_Fields)

} // namespace api
} // namespace vms
} // namespace nx
