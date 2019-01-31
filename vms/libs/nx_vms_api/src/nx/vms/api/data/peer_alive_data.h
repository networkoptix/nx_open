#pragma once

#include "data.h"
#include "peer_data.h"
#include "tran_state_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PeerAliveData: Data
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

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PeerAliveData)
