#ifndef __SERVER_ALIVE_DATA_H_
#define __SERVER_ALIVE_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"
#include "api_peer_data.h"
#include "api_fwd.h"
#include "tran_state_data.h"

namespace ec2
{


    struct ApiPeerAliveData: ApiData
    {
        ApiPeerAliveData(): isAlive(false) {}
        ApiPeerAliveData(const ApiPeerData& peer, bool isAlive): peer(peer), isAlive(isAlive) {}

        ApiPeerData peer;
        bool isAlive;
        QnTranState persistentState;
    };
#define ApiPeerAliveData_Fields (peer)(isAlive)(persistentState)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerAliveData);

#endif // __SERVER_ALIVE_DATA_H_
