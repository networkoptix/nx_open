/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/buffer.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/stoppable.h>

#include "listening_peer_pool.h"
#include "statistics/connection_statistics_info.h"

namespace nx {
namespace hpm {

namespace conf { class Settings; }

class AbstractRelayClusterClient;

/**
 * @note Object can be safely freed while in onFsmFinishedEventHandler handler.
 *     Otherwise, one has to stop it with QnStoppableAsync::pleaseStop
 */
class UDPHolePunchingConnectionInitiationFsm:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;

public:
    /** 
     * @note onFsmFinishedEventHandler is allowed to free
     *     UDPHolePunchingConnectionInitiationFsm instance.
     */
    UDPHolePunchingConnectionInitiationFsm(
        nx::String connectionID,
        const ListeningPeerData& serverPeerData,
        std::function<void(api::NatTraversalResultCode)> onFsmFinishedEventHandler,
        const conf::Settings& settings,
        AbstractRelayClusterClient* const relayClusterClient);
    virtual ~UDPHolePunchingConnectionInitiationFsm() override;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

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

    stats::ConnectSession statisticsInfo() const;

private:
    enum class State
    {
        init,
        /** connectionReqiested indication has been sent to the server */
        waitingServerPeerUDPAddress,
        /** reported server's UDP address to the client, waiting for result */
        waitingConnectionResult,
        fini,
    };

    State m_state;
    nx::String m_connectionID;
    std::function<void(api::NatTraversalResultCode)> m_onFsmFinishedEventHandler;
    const conf::Settings& m_settings;
    AbstractRelayClusterClient* const m_relayClusterClient;
    nx::network::aio::Timer m_timer;
    ConnectionWeakRef m_serverConnectionWeakRef;
    std::function<void(api::ResultCode, api::ConnectResponse)> m_connectResponseSender;
    ListeningPeerData m_serverPeerData;
    stats::ConnectSession m_sessionStatisticsInfo;
    boost::optional<std::pair<api::ResultCode, api::ConnectResponse>> m_cachedConnectResponse;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;

    virtual void stopWhileInAioThread() override;
    
    void processConnectRequest(
        const ConnectionStrongRef& originatingPeerConnection,
        api::ConnectRequest request,
        std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender);
    bool connectResponseHasAlreadyBeenSent() const;
    void repeatConnectResponse(
        std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender);
    void updateSessionStatistics(
        const ConnectionStrongRef& originatingPeerConnection,
        const api::ConnectRequest& connectRequest);
    bool notifyListeningPeerAboutConnectRequest(
        const ConnectionStrongRef& originatingPeerConnection,
        const api::ConnectRequest& connectRequest);
    nx::stun::Message prepareConnectionRequestedIndication(
        const ConnectionStrongRef& originatingPeerConnection,
        const api::ConnectRequest& connectRequest);
    void noConnectionAckOnTime();

    void processConnectionAckRequest(
        const ConnectionStrongRef& connection,
        api::ConnectionAckRequest request,
        std::function<void(api::ResultCode)> completionHandler);
    void onRelayInstanceSearchCompletion(
        api::ConnectResponse connectResponse,
        boost::optional<QUrl> relayInstanceUrl,
        std::function<void(api::ResultCode)> completionHandler);
    api::ConnectResponse prepareConnectResponse(
        const api::ConnectionAckRequest& connectionAckRequest,
        std::list<SocketAddress> tcpEndpoints,
        boost::optional<QUrl> relayInstanceUrl);
    void sendConnectResponse(
        api::ResultCode resultCode,
        api::ConnectResponse connectResponse);

    void processConnectionResultRequest(
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    void done(api::ResultCode result);

    static const char* toString(State);

    UDPHolePunchingConnectionInitiationFsm(UDPHolePunchingConnectionInitiationFsm&&) = delete;
    UDPHolePunchingConnectionInitiationFsm&
        operator=(UDPHolePunchingConnectionInitiationFsm&&) = delete;
};

} // namespace hpm
} // namespace nx
