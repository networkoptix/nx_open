/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef MEDIASERVER_EMULATOR_H
#define MEDIASERVER_EMULATOR_H

#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator_address_publisher.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/stoppable.h>

#include "../cloud_data_provider.h"


namespace nx {
namespace hpm {

class MediaServerEmulator
:
    private QnStoppableAsync,
    public StreamConnectionHolder<stun::MessagePipeline>
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
        const SocketAddress& mediatorEndpoint,
        AbstractCloudDataProvider::System systemData,
        nx::String serverName = nx::String());
    virtual ~MediaServerEmulator();

    /** Attaches to a local port and */
    bool start();
    nx::String serverId() const;
    void setServerId(nx::String serverId);
    /** returns serverId.systemId */
    nx::String fullName() const;
    /** Server endpoint */
    SocketAddress endpoint() const;

    nx::hpm::api::ResultCode bind();
    nx::hpm::api::ResultCode listen() const;

    /** Address of connection to mediator */
    SocketAddress mediatorConnectionLocalAddress() const;
    /** server's UDP address for hole punching */
    SocketAddress udpHolePunchingEndpoint() const;

    /** if \a handler returns \a false then no indication will NOT be processed */
    void setOnConnectionRequestedHandler(
        std::function<ActionToTake(api::ConnectionRequestedEvent)> handler);
    void setConnectionAckResponseHandler(
        std::function<ActionToTake(api::ResultCode)> handler);

    void setCloudSystemIdForModuleInformation(
        boost::optional<nx::String> cloudSystemId);
    void setServerIdForModuleInformation(
        boost::optional<nx::String> serverId);

    nx::hpm::api::ResultCode updateTcpAddresses(std::list<SocketAddress> addresses);

private:
    nx::network::aio::Timer m_timer;
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    nx_http::HttpStreamSocketServer m_httpServer;
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
    std::unique_ptr<stun::MessagePipeline> m_stunPipeline;
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
        AbstractStreamSocket* acceptedSocket);
    void onMessageReceived(nx::stun::Message message);

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        stun::MessagePipeline* connection) override;
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    MediaServerEmulator(const MediaServerEmulator&);
    MediaServerEmulator& operator=(const MediaServerEmulator&);
};

}   //hpm
}   //nx

#endif  //MEDIASERVER_EMULATOR_H
