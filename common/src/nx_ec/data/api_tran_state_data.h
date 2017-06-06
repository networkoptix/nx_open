#ifndef __TRAN_STATE_DATA_H_
#define __TRAN_STATE_DATA_H_

#include "api_fwd.h"
#include "api_globals.h"
#include "api_data.h"
#include "api_peer_data.h"

namespace ec2
{

struct QnTranState {
    /** map<(peer, db), persistent sequence> */
    QMap<ApiPersistentIdData, qint32> values;
};
/** @returns \a true if \a left represents more transactions then \a right. */
bool operator<(const QnTranState& left, const QnTranState& right);
bool operator>(const QnTranState& left, const QnTranState& right);

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

inline void serialize_field(const ec2::QnTranState &, QVariant *) { return; }
inline void deserialize_field(const QVariant &, ec2::QnTranState *) { return; }

#endif // __TRAN_STATE_DATA_H_
