// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator_address_publisher.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/stoppable.h>
#include "nx/network/http/test_http_server.h"

namespace nx::network::cloud::test {

struct SystemCredentials
{
    std::string id;
    std::string authKey;
};

struct ServerTweak
{
    bool bindEndpoint = true;
    bool listenToConnect = true;
};

/**
 * Cloud connect listening peer.
 * Provides a way to alter accepting peer functionality to emulate various issues on the
 * listening peer side.
 */
class NX_NETWORK_API TestListeningPeer:
    public network::aio::BasicPollable
{
public:
    enum class ActionToTake
    {
        ignoreIndication,
        closeConnectionToMediator,
        establishUdtConnection,
        ignoreSyn,
        sendBadSynAck,
        doNotAnswerTunnelChoiceNotification,
        proceedWithConnection,
    };

    /**
     * @param serverName If empty, name is generated
     */
    TestListeningPeer(
        const network::SocketAddress& mediatorUdpEndpoint,
        const nx::utils::Url& mediatorTcpUrl,
        SystemCredentials systemData,
        std::string serverName = std::string());
    virtual ~TestListeningPeer();

    TestListeningPeer(const TestListeningPeer&) = delete;
    TestListeningPeer& operator=(const TestListeningPeer&) = delete;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /** Attaches to a local port and */
    bool start(bool listenForConnectRequests = true);
    std::string serverId() const;
    void setServerId(std::string serverId);
    /** returns serverId.systemId */
    std::string fullName() const;
    /** Server endpoint */
    network::SocketAddress endpoint() const;

    nx::hpm::api::ResultCode bind();
    std::pair<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse> listen() const;

    /** Address of connection to mediator */
    network::SocketAddress mediatorConnectionLocalAddress() const;
    /** server's UDP address for hole punching */
    network::SocketAddress udpHolePunchingEndpoint() const;

    /** if \a handler returns \a false then no indication will NOT be processed */
    void setOnConnectionRequestedHandler(
        std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)> handler);

    void setConnectionAckResponseHandler(
        std::function<ActionToTake(nx::hpm::api::ResultCode)> handler);

    void setCloudSystemIdForModuleInformation(
        std::optional<std::string> cloudSystemId);

    void setServerIdForModuleInformation(
        std::optional<std::string> serverId);

    nx::hpm::api::ResultCode updateTcpAddresses(std::vector<network::SocketAddress> addresses);
    hpm::api::MediatorConnector& mediatorConnector();
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection();

    /**
     * @param mask Bitset of network::cloud::ConnectType enumeration values.
     */
    void setCloudConnectionMethodMask(int mask);

    bool registerStaticProcessor(
        const std::string& path,
        nx::Buffer msgBody,
        const std::string& mimeType,
        const nx::network::http::Method& method);

    //---------------------------------------------------------------------------------------------

    static std::unique_ptr<TestListeningPeer> buildServer(
        const SystemCredentials& system,
        std::string name,
        ServerTweak serverConf,
        const SocketAddress& mediatorStunUdpEndpoint,
        const nx::utils::Url& mediatorTcpUrl);

private:
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    nx::network::http::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx::network::http::HttpStreamSocketServer> m_httpServer;
    SystemCredentials m_systemData;
    std::string m_serverId;
    nx::network::SocketAddress m_mediatorUdpEndpoint;
    std::shared_ptr<nx::hpm::api::MediatorServerTcpConnection> m_serverClient;
    std::unique_ptr<nx::hpm::api::MediatorServerUdpConnection> m_mediatorUdpClient;
    std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)>
        m_onConnectionRequestedHandler;
    std::function<ActionToTake(nx::hpm::api::ResultCode)> m_connectionAckResponseHandler;
    nx::hpm::api::ConnectionRequestedEvent m_connectionRequestedData;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtStreamSocket;
    std::unique_ptr<nx::network::UdtStreamServerSocket> m_udtStreamServerSocket;
    std::unique_ptr<network::stun::MessagePipeline> m_stunPipeline;
    ActionToTake m_action;
    int m_cloudConnectionMethodMask;
    std::unique_ptr<network::cloud::MediatorAddressPublisher> m_mediatorAddressPublisher;
    std::optional<std::string> m_cloudSystemIdForModuleInformation;
    std::optional<std::string> m_serverIdForModuleInformation;

    void onConnectionRequested(
        nx::hpm::api::ConnectionRequestedEvent connectionRequestedData);
    void onConnectionAckResponseReceived(nx::hpm::api::ResultCode resultCode);
    void onUdtConnectDone(SystemError::ErrorCode errorCode);
    void onUdtConnectionAccepted(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> acceptedSocket);
    void onMessageReceived(network::stun::Message message);

    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network::cloud::test
