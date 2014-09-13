#ifndef __TRAN_STATE_DATA_H_
#define __TRAN_STATE_DATA_H_

#include "api_fwd.h"
#include "api_globals.h"
#include "api_data.h"

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

struct QnTranStateResponse
{
    int result;
};
#define QnTranStateResponse_Fields (result)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTranStateKey)(QnTranState)(QnTranStateResponse), (json)(ubjson))

}

#endif // __TRAN_STATE_DATA_H_
