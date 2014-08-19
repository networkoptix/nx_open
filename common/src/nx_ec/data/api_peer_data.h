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

        ApiPeerData(QUuid id, Qn::PeerType peerType, Qn::SerializationFormat dataFormat = Qn::UbjsonFormat):
            id(id),
            peerType(peerType),
            dataFormat(dataFormat) 
        {}

        bool operator==(const ApiPeerData& other) const {
            return id == other.id && peerType == other.peerType && dataFormat == other.dataFormat;
        }

        bool isClient() const {
            return peerType != Qn::PT_Server;
        }

        /** Unique ID of the peer. */ 
        QUuid id;

        /** Type of the peer. */
        Qn::PeerType peerType;

        /** Preferred client data serialization format */
        Qn::SerializationFormat dataFormat;
    };
    typedef QSet<QUuid> QnPeerSet;

#define ApiPeerData_Fields (id)(peerType)(dataFormat)

}

#endif // __API_PEER_DATA_H_
