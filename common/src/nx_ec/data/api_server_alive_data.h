#ifndef __SERVER_ALIVE_DATA_H_
#define __SERVER_ALIVE_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"
#include "api_peer_data.h"
#include "api_fwd.h"

namespace ec2
{

    struct QnTranStateKey {
        QnTranStateKey() {}
        QnTranStateKey(QUuid peerID, QUuid dbID): peerID(peerID), dbID(dbID) {}
        QUuid peerID;
        QUuid dbID;

        bool operator<(const QnTranStateKey& other) const {
            if (peerID != other.peerID)
                return peerID < other.peerID;
            return dbID < other.dbID;
        }
        bool operator>(const QnTranStateKey& other) const {
            if (peerID != other.peerID)
                return peerID > other.peerID;
            return dbID > other.dbID;
        }
    };
#define QnTranStateKey_Fields (peerID)(dbID)

    struct QnTranState {
        QMap<QnTranStateKey, qint32> values;
    };
#define QnTranState_Fields (values)

    struct ApiPeerAliveData: ApiData
    {
        ApiPeerAliveData(): isAlive(false) {}
        ApiPeerAliveData(const ApiPeerData& peer, bool isAlive): peer(peer), isAlive(isAlive) {}

        ApiPeerData peer;
        bool isAlive;
        QnTranState persistentState;
    };
#define ApiPeerAliveData_Fields (peer)(isAlive)(persistentState)

    struct QnTranStateResponse
    {
        int result;
    };
#define QnTranStateResponse_Fields (result)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTranStateKey)(QnTranState)(QnTranStateResponse), (json)(ubjson))

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerAliveData);

#endif // __SERVER_ALIVE_DATA_H_
