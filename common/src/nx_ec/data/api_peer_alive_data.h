#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include "api_peer_data.h"
#include "api_fwd.h"
#include "api_tran_state_data.h"

namespace ec2 {

struct ApiPeerAliveData: ApiData
{
    ApiPeerAliveData(): isAlive(false) {}
    ApiPeerAliveData(const ApiPeerData& peer, bool isAlive): peer(peer), isAlive(isAlive) {}

    ApiPeerData peer;
    bool isAlive;
    QnTranState persistentState;
    QnTranState runtimeState;
};
#define ApiPeerAliveData_Fields (peer)(isAlive)(persistentState)(runtimeState)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerAliveData);
