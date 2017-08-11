#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include <nx_ec/ec_proto_version.h>
#include <nx/network/app_info.h>
#include <nx/network/http/http_types.h>

namespace ec2 {

struct ApiPersistentIdData: ApiIdData
{
    ApiPersistentIdData(): ApiIdData() {}

    ApiPersistentIdData(
        const QnUuid& id,
        const QnUuid& persistentId)
    :
        ApiIdData(id),
        persistentId(persistentId)
    {
    }

    bool operator<(const ApiPersistentIdData& other) const
    {
        if (id != other.id)
            return id < other.id;
        return persistentId < other.persistentId;
    }
    bool operator>(const ApiPersistentIdData& other) const
    {
        return other < (*this);
    }

    bool isNull() const { return id.isNull();  }

    /** Unique persistent Db ID of the peer. Empty for clients */
    QnUuid persistentId;

    bool operator==(const ApiPersistentIdData& other) const
    {
        return id == other.id && persistentId == other.persistentId;
    }
};

#define ApiPersistentIdData_Fields ApiIdData_Fields (persistentId)

struct ApiPeerData: ApiPersistentIdData
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
        ApiPersistentIdData(id, QnUuid()),
        instanceId(instanceId),
        peerType(peerType),
        dataFormat(dataFormat)
    {}

    ApiPeerData(
        const ApiPersistentIdData& peer,
        Qn::PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat)
        :
        ApiPersistentIdData(peer),
        peerType(peerType),
        dataFormat(dataFormat)
    {}

    ApiPeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        const QnUuid& persistentId,
        Qn::PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat)
    :
        ApiPersistentIdData(id, persistentId),
        instanceId(instanceId),
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
            return peerType != Qn::PT_NotDefined &&
                peerType != Qn::PT_Server &&
                peerType != Qn::PT_CloudServer &&
                peerType != Qn::PT_OldServer;
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

    bool isCloudServer() const
    {
        return peerType == Qn::PT_CloudServer;
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

#define ApiPeerData_Fields ApiPersistentIdData_Fields (instanceId)(peerType)(dataFormat)

struct ApiPeerDataEx: public ApiPeerData
{
    ApiPeerDataEx(): ApiPeerData() {}
    ApiPeerDataEx(const ApiPeerData& data) : ApiPeerData(data) {}

    QnUuid systemId;
    QString cloudHost = nx::network::AppInfo::defaultCloudHost();
    qint64 identityTime = 0;
    int aliveUpdateIntervalMs = 0;
    int protoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;
};

#define ApiPeerDataEx_Fields ApiPeerData_Fields (systemId)(cloudHost)(identityTime)(aliveUpdateIntervalMs)(protoVersion)

ec2::ApiPeerDataEx deserializeFromRequest(const nx_http::Request& request);
void serializeToResponse(
    nx_http::Response* response,
    ec2::ApiPeerDataEx localPeer,
    Qn::SerializationFormat dataFormat);

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerData);
