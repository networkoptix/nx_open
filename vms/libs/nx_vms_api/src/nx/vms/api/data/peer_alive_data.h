// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"
#include "peer_data.h"
#include "tran_state_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PeerAliveData
{
    PeerAliveData() = default;
    PeerAliveData(const PeerData& peer, bool isAlive): peer(peer), isAlive(isAlive) {}

    PeerData peer;
    bool isAlive = false;
    TranState persistentState;
    TranState runtimeState;
};
#define PeerAliveData_Fields \
    (peer) \
    (isAlive) \
    (persistentState) \
    (runtimeState)
NX_VMS_API_DECLARE_STRUCT(PeerAliveData)

} // namespace api
} // namespace vms
} // namespace nx
