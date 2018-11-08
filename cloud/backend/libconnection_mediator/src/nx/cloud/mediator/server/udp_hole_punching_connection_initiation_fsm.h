#pragma once

#include <functional>
#include <optional>

#include <nx/network/aio/async_operation_wrapper.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/buffer.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/stoppable.h>

#include "../relay/abstract_relay_cluster_client.h"
#include "../statistics/connection_statistics_info.h"
#include "../listening_peer_pool.h"

namespace nx {
namespace hpm {

namespace conf { class Settings; }

/**
 * NOTE: Object can be safely freed while in onFsmFinishedEventHandler handler.
 *     Otherwise, one has to stop it with QnStoppableAsync::pleaseStop
 */
class UDPHolePunchingConnectionInitiationFsm:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;

public:
    using ConnectCompletionHandler =
        std::function<void(api::ResultCode, api::ConnectResponse)>;

    /**
     * NOTE: onFsmFinishedEventHandler is allowed to free
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
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        ConnectCompletionHandler completionHandler);

    void onConnectionAckRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
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
        /** ConnectionReqiested indication has been sent to the server. */
        waitingServerPeerUDPAddress,
        resolvingServersRelayInstance,
        /** Reported server's UDP address to the client, waiting for result. */
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
    ConnectCompletionHandler m_connectResponseSender;
    const std::list<network::SocketAddress> m_serverPeerEndpoints;
    const nx::String m_serverPeerHostName;
    const api::CloudConnectVersion m_serverPeerCloudConnectVersion;
    api::ConnectionMethods m_serverPeerConnectionMethods;
    stats::ConnectSession m_sessionStatisticsInfo;
    api::ConnectResponse m_preparedConnectResponse;
    std::optional<std::pair<api::ResultCode, api::ConnectResponse>> m_cachedConnectResponse;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    std::function<void(api::ResultCode)> m_connectionAckCompletionHandler;
    api::CloudConnectVersion m_originatingPeerCloudConnectVersion;
    nx::network::aio::AsyncOperationWrapper<
        decltype(&AbstractRelayClusterClient::findRelayInstancePeerIsListeningOn)
    > m_findRelayInstanceFunc;

    virtual void stopWhileInAioThread() override;

    void processConnectRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        ConnectCompletionHandler connectResponseSender);

    void processUdpConnectRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        ConnectCompletionHandler connectResponseSender);

    void processTcpConnectRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        ConnectCompletionHandler connectResponseSender);

    bool connectResponseHasAlreadyBeenSent() const;

    void repeatConnectResponse(
        ConnectCompletionHandler connectResponseSender);

    void updateSessionStatistics(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest);

    bool notifyListeningPeerAboutConnectRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest);

    nx::network::stun::Message prepareConnectionRequestedIndication(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest);

    void noConnectionAckOnTime();

    void processConnectionAckRequest(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectionAckRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    bool initiateCloudConnect(api::ConnectionAckRequest connectionAck);

    void initiateRelayInstanceSearch();

    void findRelayInstance(
        RelayInstanceSearchCompletionHandler handler);

    void finishConnect();

    void onRelayInstanceSearchCompletion(std::optional<QUrl> relayInstanceUrl);

    api::ConnectResponse prepareConnectResponse(
        const api::ConnectionAckRequest& connectionAckRequest,
        std::list<network::SocketAddress> tcpEndpoints,
        std::optional<QUrl> relayInstanceUrl);

    void sendConnectResponse(
        api::ResultCode resultCode,
        api::ConnectResponse connectResponse);
    
    void fixConnectResponseForBuggyClient(
        api::ResultCode resultCode,
        api::ConnectResponse* const connectResponse);

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
