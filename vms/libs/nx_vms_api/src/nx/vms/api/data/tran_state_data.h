#pragma once

#include "peer_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API TranState
{
    /** map<(peer, db), persistent sequence> */
    QMap<nx::vms::api::PersistentIdData, qint32> values;

    /** @returns \a true if \a left represents more transactions then \a right. */
    bool operator<(const TranState& right) const;

    bool operator>(const TranState& right) const { return right < *this; }
};
#define TranState_Fields (values)

struct NX_VMS_API TranStateResponse
{
    int result = 0;
};
#define TranStateResponse_Fields (result)

struct NX_VMS_API TranSyncDoneData
{
    int result = 0;
};
#define TranSyncDoneData_Fields (result)

struct NX_VMS_API SyncRequestData
{
    TranState persistentState;
    TranState runtimeState;
};
#define SyncRequestData_Fields (persistentState)(runtimeState)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::TranState)
Q_DECLARE_METATYPE(nx::vms::api::TranStateResponse)
Q_DECLARE_METATYPE(nx::vms::api::TranSyncDoneData)
Q_DECLARE_METATYPE(nx::vms::api::SyncRequestData)
