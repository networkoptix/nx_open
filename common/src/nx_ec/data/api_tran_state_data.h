#ifndef __TRAN_STATE_DATA_H_
#define __TRAN_STATE_DATA_H_

#include "api_fwd.h"
#include "api_globals.h"
#include "api_data.h"

namespace ec2
{


struct QnTranStateKey {
    QnTranStateKey() {}
    QnTranStateKey(QnUuid peerID, QnUuid dbID): peerID(peerID), dbID(dbID) {}
    QnUuid peerID;
    QnUuid dbID;

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
    QnTranStateResponse(): result(0) {}
    int result;
};
#define QnTranStateResponse_Fields (result)

struct ApiTranSyncDoneData
{
    ApiTranSyncDoneData(): result(0) {}
    int result;
};
#define ApiTranSyncDoneData_Fields (result)


struct ApiSyncRequestData {
    QnTranState persistentState;
    QnTranState runtimeState;
};
#define ApiSyncRequestData_Fields (persistentState)(runtimeState)

}

#endif // __TRAN_STATE_DATA_H_
