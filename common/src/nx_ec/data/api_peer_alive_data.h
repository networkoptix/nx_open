#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include "api_fwd.h"
#include "api_tran_state_data.h"

#include <nx/vms/api/data/peer_data.h>

namespace ec2 {

struct ApiPeerAliveData: nx::vms::api::Data
{
    ApiPeerAliveData() = default;
    ApiPeerAliveData(const nx::vms::api::PeerData& peer, bool isAlive): peer(peer), isAlive(isAlive) {}

    nx::vms::api::PeerData peer;
    bool isAlive = false;
    QnTranState persistentState;
    QnTranState runtimeState;
};
#define ApiPeerAliveData_Fields (peer)(isAlive)(persistentState)(runtimeState)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerAliveData);
