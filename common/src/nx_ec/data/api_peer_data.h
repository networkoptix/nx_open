#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"

namespace ec2 {

struct ApiPeerData: ApiIdData
{
    ApiPeerData():
        dataFormat(Qn::UbjsonFormat)
    {}

    ApiPeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        Qn::PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat)
        :
        ApiIdData(id),
        instanceId(instanceId),
        peerType(peerType),
        dataFormat(dataFormat)
    {}

    bool operator==(const ApiPeerData& other) const
    {
        return id == other.id
            && instanceId == other.instanceId
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

    /** Unique running instance ID of the peer. */
    QnUuid instanceId;

    /** Type of the peer. */
    Qn::PeerType peerType;

    /** Preferred client data serialization format */
    Qn::SerializationFormat dataFormat;
};
typedef QSet<QnUuid> QnPeerSet;

#define ApiPeerData_Fields ApiIdData_Fields (instanceId)(peerType)(dataFormat)

} // namespace ec2
