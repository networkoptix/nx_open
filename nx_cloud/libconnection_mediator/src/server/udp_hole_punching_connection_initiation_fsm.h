/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>

#include <nx/network/aio/timer.h>
#include <nx/network/buffer.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <utils/common/stoppable.h>

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

namespace conf {
    class Settings;
}   // namespace conf

/**
    \note Object can be safely freed while in \a onFsmFinishedEventHandler handler.
        Otherwise, one has to stop it with \a QnStoppableAsync::pleaseStop
*/
class UDPHolePunchingConnectionInitiationFsm
:
    public QnStoppableAsync
{
public:
    /** 
        \note \a onFsmFinishedEventHandler is allowed to free 
            \a UDPHolePunchingConnectionInitiationFsm instance
    */
    UDPHolePunchingConnectionInitiationFsm(
        nx::String connectionID,
        const ListeningPeerPool::ConstDataLocker& serverPeerDataLocker,
        std::function<void(api::ResultCode)> onFsmFinishedEventHandler,
        const conf::Settings& settings);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler);

    void onConnectRequest(
        const ConnectionStrongRef& connection,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
    void onConnectionAckRequest(
        const ConnectionStrongRef& connection,
        api::ConnectionAckRequest request,
        std::function<void(api::ResultCode)> completionHandler);
    void onConnectionResultRequest(
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

private:
    enum class State
    {
        init,
        /** connectionReqiested indication has been sent to the server */
        waitingServerPeerUDPAddress,
        /** reported server's UDP address to the client, waiting for result */
        waitingConnectionResult,
        fini
    };

    State m_state;
    nx::String m_connectionID;
    std::function<void(api::ResultCode)> m_onFsmFinishedEventHandler;
    const conf::Settings& m_settings;
    nx::network::aio::Timer m_timer;
    ConnectionWeakRef m_serverConnectionWeakRef;
    std::function<void(api::ResultCode, api::ConnectResponse)> m_connectResponseSender;
    
    void onServerConnectionClosed();
    void done(api::ResultCode result);

    UDPHolePunchingConnectionInitiationFsm(UDPHolePunchingConnectionInitiationFsm&&);
    UDPHolePunchingConnectionInitiationFsm&
        operator=(UDPHolePunchingConnectionInitiationFsm&&);
};

} // namespace hpm
} // namespace nx
