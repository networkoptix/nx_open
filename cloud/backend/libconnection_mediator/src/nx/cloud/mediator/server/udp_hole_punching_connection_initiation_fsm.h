#pragma once

#include <functional>
#include <optional>
#include <vector>

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

enum class State
{
    init,
    sendingConnectionRequestedIndication,
    /** ConnectionRequested indication has been sent to the server. */
    waitingServerPeerUdpAddress,
    resolvingServersRelayInstance,
    /** Reported server's UDP address to the client, waiting for result. */
    waitingConnectionResult,
    fini,
};

constexpr const char* toString(State);

enum class Event
{
    connectOverUdpRequestReceived,
    connectOverTcpRequestReceived,
    connectionRequestedSent,
    connectionRequestedSendFailed,
    connectionAckReceived,
    connectionAckWaitTimedOut,
    relayResolveCompleted,
    connectionResultReceived,
    connectionResultWaitTimedOut,
};

constexpr const char* toString(Event);

//-------------------------------------------------------------------------------------------------

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
        nx::String connectionId,
        const ListeningPeerData& serverPeerData,
        std::function<void(api::NatTraversalResultCode)> onFsmFinishedEventHandler,
        const conf::Settings& settings,
        AbstractRelayClusterClient* const relayClusterClient);

    virtual ~UDPHolePunchingConnectionInitiationFsm() override;

    UDPHolePunchingConnectionInitiationFsm(UDPHolePunchingConnectionInitiationFsm&&) = delete;

    UDPHolePunchingConnectionInitiationFsm&
        operator=(UDPHolePunchingConnectionInitiationFsm&&) = delete;

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
    struct ConnectResponseSenderContext
    {
        network::TransportProtocol networkProtocol;
        ConnectCompletionHandler sendResponseFunc;

        ConnectResponseSenderContext() = delete;
    };

    State m_state;
    nx::String m_connectionId;
    std::function<void(api::NatTraversalResultCode)> m_onFsmFinishedEventHandler;
    const conf::Settings& m_settings;
    AbstractRelayClusterClient* const m_relayClusterClient;
    nx::network::aio::Timer m_timer;
    ConnectionWeakRef m_serverConnectionWeakRef;

    std::optional<RequestSourceDescriptor> m_clientEndpoint;
    api::ConnectRequest m_connectRequest;
    api::ConnectionAckRequest m_connectionAckRequest;

    std::vector<ConnectResponseSenderContext> m_connectResponseSenders;
    const std::vector<network::SocketAddress> m_serverPeerEndpoints;
    const nx::String m_serverPeerHostName;
    const api::CloudConnectVersion m_serverPeerCloudConnectVersion;
    api::ConnectionMethods m_serverPeerConnectionMethods;
    stats::ConnectSession m_sessionStatisticsInfo;
    std::optional<nx::String> m_trafficRelayUrl;
    std::optional<std::tuple<api::ResultCode, api::ConnectResponse>> m_cachedConnectResponse;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;
    std::function<void(api::ResultCode)> m_connectionAckCompletionHandler;
    api::CloudConnectVersion m_originatingPeerCloudConnectVersion;
    nx::network::aio::AsyncOperationWrapper<
        decltype(&AbstractRelayClusterClient::findRelayInstanceForClient)
    > m_findRelayInstanceFunc;
    std::optional<api::ConnectionResultRequest> m_connectResultReport;

    virtual void stopWhileInAioThread() override;

    //---------------------------------------------------------------------------------------------
    // FSM implementation.

    /**
     * Changes state (if the transition is valid) and invokes onEnter/onExit handlers.
     * @return true if event is valid in the current state. false otherwise
     */
    bool changeState(Event event);

    /**
     * @param Resulting state.
     */
    std::optional<State> getDestinationState(State from, Event event) const;

    void invokeOnExitingStateHandler(State state);

    void invokeOnEnteredStateHandler(State state);

    //---------------------------------------------------------------------------------------------
    // Events.

    void connectRequestReceived(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectRequest request,
        ConnectCompletionHandler connectResponseSender);

    void connectionRequestedNotificationSent();

    void connectionRequestedNotificationFailed();

    void connectionAckReceived(
        const RequestSourceDescriptor& requestSourceDescriptor,
        api::ConnectionAckRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    void noConnectionAckOnTime();

    void relayInstanceResolveCompleted(
        nx::cloud::relay::api::ResultCode resultCode,
        nx::utils::Url relayInstanceUrl);

    void connectionResultReceived(
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

    void connectionResultWaitTimedOut();

    //---------------------------------------------------------------------------------------------
    // Actions

    void sendConnectionRequestedNotification();

    void startNoConnectionAckTimer();

    void stopNoConnectionAckTimer();

    void sendConnectResponse();

    void sendConnectionAckResponse();

    void startConnectionResultWaitTimer();

    void stopConnectionResultWaitTimer();

    void exitFsmIfReportHasAlreadyBeenReceived();

    void reportFsmCompletion();

    //---------------------------------------------------------------------------------------------

    void updateSessionStatistics(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest);

    nx::network::stun::Message prepareConnectionRequestedIndication(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest);

    void initiateRelayInstanceSearch();

    void findRelayInstance(
        RelayInstanceSearchCompletionHandler handler);

    std::tuple<api::ResultCode, api::ConnectResponse> prepareConnectResponse();

    void fixConnectResponseForBuggyClient(
        api::ResultCode resultCode,
        api::ConnectResponse* const connectResponse);

    void sendConnectResponse(
        ConnectResponseSenderContext& senderContext,
        const std::tuple<api::ResultCode, api::ConnectResponse>& response);
};

} // namespace hpm
} // namespace nx
