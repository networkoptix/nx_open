// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

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

    /** @return true if difference of sets *this and other is not empty. false otherwise. */
    bool containsDataMissingIn(const TranState& other) const;
};
#define TranState_Fields (values)
NX_VMS_API_DECLARE_STRUCT(TranState)

struct NX_VMS_API TranStateResponse
{
    int result = 0;
};
#define TranStateResponse_Fields (result)
NX_VMS_API_DECLARE_STRUCT(TranStateResponse)

struct NX_VMS_API TranSyncDoneData
{
    int result = 0;
};
#define TranSyncDoneData_Fields (result)
NX_VMS_API_DECLARE_STRUCT(TranSyncDoneData)

struct NX_VMS_API SyncRequestData
{
    TranState persistentState;
    TranState runtimeState;
};
#define SyncRequestData_Fields (persistentState)(runtimeState)
NX_VMS_API_DECLARE_STRUCT(SyncRequestData)

} // namespace api
} // namespace vms
} // namespace nx
