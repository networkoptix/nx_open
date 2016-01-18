/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>

#include <nx/network/buffer.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>


namespace nx {
namespace hpm {

class UDPHolePunchingConnectionInitiationFsm
{
public:
    UDPHolePunchingConnectionInitiationFsm(
        nx::String connectionID,
        const & serverPeerDataLocker,
        std::function<void()> onFsmFinishedEventHandler);

    void onConnectRequest(
        const ConnectionStrongRef& connection,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
    void onConnectionAckRequest(
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
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
    std::function<void()> m_onFsmFinishedEventHandler;
    std::unique_ptr<AbstractStreamSocket> m_timer;
    
    void sessionTimedout();
    void onServerConnectionClosed();

    UDPHolePunchingConnectionInitiationFsm(UDPHolePunchingConnectionInitiationFsm&&);
    UDPHolePunchingConnectionInitiationFsm&
        operator=(UDPHolePunchingConnectionInitiationFsm&&);
};

} // namespace hpm
} // namespace nx
