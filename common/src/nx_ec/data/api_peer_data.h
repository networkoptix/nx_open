#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"

namespace ec2 {

struct ApiPeerIdData: ApiIdData
{
    ApiPeerIdData(): ApiIdData() {}

    ApiPeerIdData(
        const QnUuid& id,
        const QnUuid& instanceId,
        const QnUuid& persistentId)
    :
        ApiIdData(id),
        instanceId(instanceId),
        persistentId(persistentId)
    {}

    bool operator<(const ApiPeerIdData& other) const
    {
        if (id != other.id)
            return id < other.id;
        if (persistentId != other.persistentId)
            return persistentId < other.persistentId;
        return instanceId < other.instanceId;
    }

    /** Unique persistent Db ID of the peer. Empty for clients */
    QnUuid persistentId;

    /** Unique running instance ID of the peer. */
    QnUuid instanceId;

};
#define ApiPeerIdData_Fields ApiIdData_Fields (persistentId)(instanceId)

struct ApiPeerData: ApiPeerIdData
{
    ApiPeerData():
        peerType(Qn::PT_NotDefined),
        dataFormat(Qn::UbjsonFormat)
    {}

    ApiPeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        Qn::PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat)
    :
        ApiPeerIdData(id, instanceId, QnUuid()),
        peerType(peerType),
        dataFormat(dataFormat)
    {}

    bool operator==(const ApiPeerData& other) const
    {
        return id == other.id
            && instanceId == other.instanceId
            && persistentId == other.persistentId
            && peerType == other.peerType
            && dataFormat == other.dataFormat;
    }

    static bool isClient(Qn::PeerType peerType)
    {
            return peerType != Qn::PT_Server && peerType != Qn::PT_CloudServer && peerType != Qn::PT_OldServer;
    }

    bool isClient() const
    {
        return isClient(peerType);
    }

    static bool isServer(Qn::PeerType peerType)
    {
        return peerType == Qn::PT_Server || peerType == Qn::PT_OldServer;
    }

    bool isServer() const
    {
        return isServer(peerType);
    }

    bool isMobileClient() const
    {
        return isMobileClient(peerType);
    }

    static bool isMobileClient(Qn::PeerType peerType)
    {
        return
            peerType == Qn::PT_OldMobileClient ||
            peerType == Qn::PT_MobileClient;
    }

    /** Type of the peer. */
    Qn::PeerType peerType;

    /** Preferred client data serialization format */
    Qn::SerializationFormat dataFormat;
};
typedef QSet<QnUuid> QnPeerSet;

#define ApiPeerData_Fields ApiPeerIdData_Fields (peerType)(dataFormat)

} // namespace ec2
