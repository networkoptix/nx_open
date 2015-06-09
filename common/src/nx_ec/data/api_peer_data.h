#ifndef __API_PEER_DATA_H_
#define __API_PEER_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"

namespace ec2
{

    struct ApiPeerData: ApiData 
    {
        ApiPeerData()
        :
            dataFormat( Qn::UbjsonFormat )
        {}

        ApiPeerData(const QnUuid& id, const QnUuid& instanceId, Qn::PeerType peerType, Qn::SerializationFormat dataFormat = Qn::UbjsonFormat):
            id(id),
            instanceId(instanceId),
            peerType(peerType),
            dataFormat(dataFormat) 
        {}

        bool operator==(const ApiPeerData& other) const {
            return id == other.id && instanceId == other.instanceId && peerType == other.peerType && dataFormat == other.dataFormat;
        }

        bool isClient() const {
            return peerType != Qn::PT_Server;
        }

        bool isServer() const {
            return peerType == Qn::PT_Server;
        }

        bool isMobileClient() const {
            return peerType == Qn::PT_MobileClient;
        }

        /** Unique ID of the peer. */ 
        QnUuid id;

        /** Unique running instance ID of the peer. */ 
        QnUuid instanceId;

        /** Type of the peer. */
        Qn::PeerType peerType;

        /** Preferred client data serialization format */
        Qn::SerializationFormat dataFormat;
    };
    typedef QSet<QnUuid> QnPeerSet;

#define ApiPeerData_Fields (id)(instanceId)(peerType)(dataFormat)

}

#endif // __API_PEER_DATA_H_
