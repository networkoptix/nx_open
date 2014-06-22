#ifndef __API_PEER_DATA_H_
#define __API_PEER_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"

namespace ec2
{

    struct ApiPeerData: ApiData 
    {
        ApiPeerData() {}
        ApiPeerData(QnId id, Qn::PeerType peerType): id(id), peerType(peerType) {}

        bool operator==(const ApiPeerData& other) const {
            return id == other.id && peerType == other.peerType && params == other.params;
        }

        bool isClient() const {
            return peerType != Qn::PT_Server;
        }

        /** Unique ID of the peer. */ 
        QnId id;

        /** Type of the peer. */
        Qn::PeerType peerType;

        /** Additional info. */
        QMap<QString, QString> params; // todo: #GDM #VW. remove it
    };
    typedef QSet<QnId> QnPeerSet;

#define ApiPeerData_Fields (id)(peerType)(params)

}

#endif // __API_PEER_DATA_H_
