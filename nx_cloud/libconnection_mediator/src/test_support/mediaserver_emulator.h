#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator_address_publisher.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/stoppable.h>

#include "../cloud_data_provider.h"

namespace nx {
namespace hpm {

class MediaServerEmulator:
    public network::aio::BasicPollable,
    public nx::network::server::StreamConnectionHolder<network::stun::MessagePipeline>
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
        \param serverName If empty, name is generated
    */
    MediaServerEmulator(
        const network::SocketAddress& mediatorEndpoint,
        AbstractCloudDataProvider::System systemData,
        nx::String serverName = nx::String());
    virtual ~MediaServerEmulator();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    /** Attaches to a local port and */
    bool start(bool listenForConnectRequests = true);
    nx::String serverId() const;
    void setServerId(nx::String serverId);
    /** returns serverId.systemId */
    nx::String fullName() const;
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
        std::function<ActionToTake(api::ConnectionRequestedEvent)> handler);
    void setConnectionAckResponseHandler(
        std::function<ActionToTake(api::ResultCode)> handler);

    void setCloudSystemIdForModuleInformation(
        boost::optional<nx::String> cloudSystemId);
    void setServerIdForModuleInformation(
        boost::optional<nx::String> serverId);

    nx::hpm::api::ResultCode updateTcpAddresses(std::list<network::SocketAddress> addresses);
    hpm::api::MediatorConnector& mediatorConnector();
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection();

private:
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    nx::network::http::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx::network::http::HttpStreamSocketServer> m_httpServer;
    AbstractCloudDataProvider::System m_systemData;
    nx::String m_serverId;
    std::shared_ptr<nx::hpm::api::MediatorServerTcpConnection> m_serverClient;
    std::unique_ptr<nx::hpm::api::MediatorServerUdpConnection> m_mediatorUdpClient;
    std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)>
        m_onConnectionRequestedHandler;
    std::function<ActionToTake(api::ResultCode)> m_connectionAckResponseHandler;
    nx::hpm::api::ConnectionRequestedEvent m_connectionRequestedData;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtStreamSocket;
    std::unique_ptr<nx::network::UdtStreamServerSocket> m_udtStreamServerSocket;
    std::unique_ptr<network::stun::MessagePipeline> m_stunPipeline;
    ActionToTake m_action;
    const int m_cloudConnectionMethodMask;
    std::unique_ptr<network::cloud::MediatorAddressPublisher> m_mediatorAddressPublisher;
    boost::optional<nx::String> m_cloudSystemIdForModuleInformation;
    boost::optional<nx::String> m_serverIdForModuleInformation;

    void onConnectionRequested(
        nx::hpm::api::ConnectionRequestedEvent connectionRequestedData);
    void onConnectionAckResponseReceived(nx::hpm::api::ResultCode resultCode);
    void onUdtConnectDone(SystemError::ErrorCode errorCode);
    void onUdtConnectionAccepted(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<network::AbstractStreamSocket> acceptedSocket);
    void onMessageReceived(network::stun::Message message);

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        network::stun::MessagePipeline* connection) override;
    virtual void stopWhileInAioThread() override;

    MediaServerEmulator(const MediaServerEmulator&);
    MediaServerEmulator& operator=(const MediaServerEmulator&);
};

} // namespace hpm
} // namespace nx
