/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef MEDIASERVER_EMULATOR_H
#define MEDIASERVER_EMULATOR_H

#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket.h>
#include <nx/network/udt/udt_socket.h>
#include <utils/common/cpp14.h>
#include <utils/common/stoppable.h>

#include "../cloud_data_provider.h"


namespace nx {
namespace hpm {

class MediaServerEmulator
:
    private QnStoppableAsync
{
public:
    enum class ActionToTake
    {
        proceedWithConnection,
        ignoreIndication,
        closeConnectionToMediator
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
    nx::hpm::api::ResultCode registerOnMediator();
    nx::String serverId() const;
    /** returns serverId.systemId */
    nx::String fullName() const;
    /** Server endpoint */
    SocketAddress endpoint() const;

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

    void onConnectionRequested(
        nx::hpm::api::ConnectionRequestedEvent connectionRequestedData);
    void onConnectionAckResponseReceived(nx::hpm::api::ResultCode resultCode);
    void onUdtConnectDone(SystemError::ErrorCode errorCode);
    void onUdtConnectionAccepted(
        SystemError::ErrorCode errorCode,
        AbstractStreamSocket* acceptedSocket);
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    MediaServerEmulator(const MediaServerEmulator&);
    MediaServerEmulator& operator=(const MediaServerEmulator&);
};

}   //hpm
}   //nx

#endif  //MEDIASERVER_EMULATOR_H
