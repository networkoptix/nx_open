#include "mediaserver_emulator.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/cloud/data/tunnel_connection_chosen_data.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/barrier_handler.h>

#include <common/common_globals.h>
#include <network/module_information.h>
#include <rest/server/json_rest_result.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>

namespace nx {
namespace hpm {

class ApiModuleInformationHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    ApiModuleInformationHandler(
        boost::optional<nx::String> cloudSystemId,
        boost::optional<nx::String> serverIdForModuleInformation)
        :
        m_cloudSystemId(std::move(cloudSystemId)),
        m_serverIdForModuleInformation(std::move(serverIdForModuleInformation))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler handler) override
    {
        QnJsonRestResult restResult;
        if (!m_serverIdForModuleInformation)
        {
            restResult.error = QnJsonRestResult::Error::CantProcessRequest;
            restResult.errorString = lit("I am not a server");
        }
        else
        {
            QnModuleInformation moduleInformation;
            moduleInformation.id = QnUuid::fromStringSafe(m_serverIdForModuleInformation.get());
            if (m_cloudSystemId)
                moduleInformation.cloudSystemId = m_cloudSystemId.get();

            restResult.setReply(moduleInformation);
        }

        std::unique_ptr<nx_http::AbstractMsgBodySource> bodySource =
            std::make_unique<nx_http::BufferSource>(
                Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
                QJson::serialized(restResult));

        handler({nx_http::StatusCode::ok, std::move(bodySource)});
    }

private:
    const boost::optional<nx::String> m_cloudSystemId;
    const boost::optional<nx::String> m_serverIdForModuleInformation;
};

MediaServerEmulator::MediaServerEmulator(
    const SocketAddress& mediatorEndpoint,
    AbstractCloudDataProvider::System systemData,
    nx::String serverName)
:
    m_mediatorConnector(std::make_unique<hpm::api::MediatorConnector>()),
    m_httpServer(
        nullptr,
        &m_httpMessageDispatcher,
        false,
        nx::network::NatTraversalSupport::disabled),
    m_systemData(std::move(systemData)),
    m_serverId(
        serverName.isEmpty()
        ? QnUuid::createUuid().toSimpleString().toUtf8()
        : std::move(serverName)),
    m_mediatorUdpClient(
        std::make_unique<nx::hpm::api::MediatorServerUdpConnection>(
            mediatorEndpoint,
            m_mediatorConnector.get())),
    m_action(ActionToTake::proceedWithConnection),
    m_cloudConnectionMethodMask((int)network::cloud::CloudConnectType::all),
    m_cloudSystemIdForModuleInformation(m_systemData.id),
    m_serverIdForModuleInformation(m_serverId)
{
    // Registering /api/moduleInformation handler.
    m_httpMessageDispatcher.registerRequestProcessor<ApiModuleInformationHandler>(
        "/api/moduleInformation",
        [this]()
        {
            return std::make_unique<ApiModuleInformationHandler>(
                m_cloudSystemIdForModuleInformation,
                m_serverIdForModuleInformation);
        });

    m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());

    m_mediatorConnector->mockupAddress(std::move(mediatorEndpoint));

    m_mediatorConnector->setSystemCredentials(
        api::SystemCredentials(
            m_systemData.id,
            m_serverId,
            m_systemData.authKey));
}

MediaServerEmulator::~MediaServerEmulator()
{
    if (m_mediatorConnector)
    {
        m_mediatorConnector->pleaseStopSync();
        m_mediatorConnector.reset();
    }

    pleaseStopSync();
    m_httpServer.pleaseStop();
}

bool MediaServerEmulator::start(bool listenToConnectRequests)
{
    if (!m_httpServer.bind(SocketAddress(HostAddress::localhost, 0)) ||
        !m_httpServer.listen())
    {
        return false;
    }

    m_serverClient = m_mediatorConnector->systemConnection();
    m_serverClient->bindToAioThread(m_timer.getAioThread());
    if (listenToConnectRequests)
    {
        using namespace std::placeholders;
        m_serverClient->setOnConnectionRequestedHandler(
            std::bind(&MediaServerEmulator::onConnectionRequested, this, _1));
    }

    auto client = m_mediatorConnector->systemConnection();
    client->bindToAioThread(m_timer.getAioThread());
    m_mediatorAddressPublisher = std::make_unique<network::cloud::MediatorAddressPublisher>(
        std::move(client));

    return true;
}

nx::String MediaServerEmulator::serverId() const
{
    return m_serverId;
}

void MediaServerEmulator::setServerId(nx::String serverId)
{
    m_serverId = serverId;
}

nx::String MediaServerEmulator::fullName() const
{
    return m_serverId + "." + m_systemData.id;
}

SocketAddress MediaServerEmulator::endpoint() const
{
    return m_httpServer.address();
}

nx::hpm::api::ResultCode MediaServerEmulator::bind()
{
    return updateTcpAddresses({endpoint()});
}

std::pair<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>
    MediaServerEmulator::listen() const
{
    api::ListenRequest requestData;
    requestData.systemId = m_systemData.id;
    requestData.serverId = m_serverId;
    nx::hpm::api::ResultCode resultCode = nx::hpm::api::ResultCode::ok;
    nx::hpm::api::ListenResponse response;
    std::tie(resultCode, response) = makeSyncCall<decltype(resultCode), decltype(response)>(
        std::bind(
            &nx::hpm::api::MediatorServerTcpConnection::listen,
            m_serverClient.get(),
            std::move(requestData),
            std::placeholders::_1));

    return std::make_pair(resultCode, response);
}

SocketAddress MediaServerEmulator::mediatorConnectionLocalAddress() const
{
    return m_serverClient->localAddress();
}

SocketAddress MediaServerEmulator::udpHolePunchingEndpoint() const
{
    return m_mediatorUdpClient->localAddress();
}

void MediaServerEmulator::setOnConnectionRequestedHandler(
    std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)> handler)
{
    m_onConnectionRequestedHandler = std::move(handler);
}

void MediaServerEmulator::setConnectionAckResponseHandler(
    std::function<ActionToTake(api::ResultCode)> handler)
{
    m_connectionAckResponseHandler = std::move(handler);
}

void MediaServerEmulator::setCloudSystemIdForModuleInformation(
    boost::optional<nx::String> cloudSystemId)
{
    m_cloudSystemIdForModuleInformation = cloudSystemId;
}

void MediaServerEmulator::setServerIdForModuleInformation(
    boost::optional<nx::String> serverId)
{
    m_serverIdForModuleInformation = serverId;
}

nx::hpm::api::ResultCode MediaServerEmulator::updateTcpAddresses(
    std::list<SocketAddress> addresses)
{
    utils::promise<nx::hpm::api::ResultCode> promise;
    m_mediatorAddressPublisher->updateAddresses(
        std::move(addresses),
        [&promise](nx::hpm::api::ResultCode success) { promise.set_value(success); });

    return promise.get_future().get();
}

std::unique_ptr<hpm::api::MediatorServerTcpConnection> MediaServerEmulator::mediatorConnection()
{
    return m_mediatorConnector->systemConnection();
}

void MediaServerEmulator::onConnectionRequested(
    nx::hpm::api::ConnectionRequestedEvent connectionRequestedData)
{
    using namespace std::placeholders;

    m_connectionRequestedData = connectionRequestedData;

    nx::hpm::api::ConnectionAckRequest connectionAckData;
    connectionAckData.connectSessionId = connectionRequestedData.connectSessionId;
    if ((m_cloudConnectionMethodMask & (int)network::cloud::CloudConnectType::udpHp) > 0)
        connectionAckData.connectionMethods = nx::hpm::api::ConnectionMethod::udpHolePunching;

    if (m_onConnectionRequestedHandler)
    {
        const auto actionToTake = m_onConnectionRequestedHandler(
            std::move(connectionRequestedData));
        switch (actionToTake)
        {
            case ActionToTake::proceedWithConnection:
                break;
            case ActionToTake::ignoreIndication:
                return;
            case ActionToTake::closeConnectionToMediator:
                m_serverClient.reset();
                m_mediatorAddressPublisher.reset();
                m_mediatorConnector.reset();
                return;
            default:
                NX_ASSERT(false);
        }
    }

    m_mediatorUdpClient->connectionAck(
        std::move(connectionAckData),
        std::bind(&MediaServerEmulator::onConnectionAckResponseReceived, this, _1));
}

void MediaServerEmulator::onConnectionAckResponseReceived(
    nx::hpm::api::ResultCode resultCode)
{
    ActionToTake action = ActionToTake::ignoreIndication;

    if (m_connectionAckResponseHandler)
        action = m_connectionAckResponseHandler(resultCode);

    if (action < ActionToTake::establishUdtConnection)
        return;
    m_action = action;

    if (m_connectionRequestedData.udpEndpointList.empty())
        return;

    //if (!m_mediatorUdpClient)
    if (resultCode != api::ResultCode::ok)
        return;

    //connecting to the originating peer
    auto udtStreamSocket = std::make_unique<nx::network::UdtStreamSocket>(AF_INET);
    udtStreamSocket->bindToAioThread(m_timer.getAioThread());
    auto mediatorUdpClientSocket = m_mediatorUdpClient->takeSocket();
    m_mediatorUdpClient.reset();
    if (!udtStreamSocket->bindToUdpSocket(std::move(*mediatorUdpClientSocket)) ||
        !udtStreamSocket->setNonBlockingMode(true) ||
        !udtStreamSocket->setRendezvous(true))
    {
        return;
    }

    auto udtStreamServerSocket = std::make_unique<nx::network::UdtStreamServerSocket>(AF_INET);
    udtStreamServerSocket->bindToAioThread(m_timer.getAioThread());
    if (!udtStreamServerSocket->setReuseAddrFlag(true) ||
        !udtStreamServerSocket->bind(udtStreamSocket->getLocalAddress()) ||
        !udtStreamServerSocket->setNonBlockingMode(true))
    {
        return;
    }

    m_udtStreamSocket = std::move(udtStreamSocket);
    m_udtStreamServerSocket = std::move(udtStreamServerSocket);

    using namespace std::placeholders;
    m_udtStreamSocket->connectAsync(
        m_connectionRequestedData.udpEndpointList.front(),
        std::bind(&MediaServerEmulator::onUdtConnectDone, this, _1));
    m_udtStreamServerSocket->acceptAsync(
        std::bind(&MediaServerEmulator::onUdtConnectionAccepted, this, _1, _2));
}

void MediaServerEmulator::onUdtConnectDone(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
        return;

    m_stunPipeline = std::make_unique<stun::MessagePipeline>(
        this,
        std::move(m_udtStreamSocket));
    m_udtStreamSocket.reset();
    using namespace std::placeholders;
    m_stunPipeline->setMessageHandler(
        std::bind(&MediaServerEmulator::onMessageReceived, this, _1));
    m_stunPipeline->startReadingConnection();
}

void MediaServerEmulator::onUdtConnectionAccepted(
    SystemError::ErrorCode /*errorCode*/,
    AbstractStreamSocket* acceptedSocket)
{
    delete acceptedSocket;
}

void MediaServerEmulator::onMessageReceived(
    nx::stun::Message message)
{
    if (m_action <= ActionToTake::ignoreSyn)
        return;

    if (message.header.messageClass != stun::MessageClass::request)
        return;

    if (message.header.method == stun::cc::methods::udpHolePunchingSyn)
    {
        hpm::api::UdpHolePunchingSynResponse synAckResponse;
        if (m_action <= ActionToTake::sendBadSynAck)
            synAckResponse.connectSessionId = "hren";
        else
            synAckResponse.connectSessionId = m_connectionRequestedData.connectSessionId;
        stun::Message synAckMessage(
            nx::stun::Header(
                nx::stun::MessageClass::successResponse,
                hpm::api::UdpHolePunchingSynResponse::kMethod,
                message.header.transactionId));
        synAckResponse.serialize(&synAckMessage);
        m_stunPipeline->sendMessage(std::move(synAckMessage));
    }
    else if (message.header.method == stun::cc::methods::tunnelConnectionChosen)
    {
        if (m_action <= ActionToTake::doNotAnswerTunnelChoiceNotification)
            return;
        hpm::api::TunnelConnectionChosenResponse responseData;
        stun::Message tunnelConnectionChosenResponseMessage(
            stun::Header(
                stun::MessageClass::request,
                hpm::api::TunnelConnectionChosenResponse::kMethod));
        responseData.serialize(&tunnelConnectionChosenResponseMessage);
        m_stunPipeline->sendMessage(std::move(tunnelConnectionChosenResponseMessage));
    }
    else
    {
        return;
    }
}

void MediaServerEmulator::closeConnection(
    SystemError::ErrorCode /*closeReason*/,
    stun::MessagePipeline* connection)
{
    NX_ASSERT(connection == m_stunPipeline.get());
}

void MediaServerEmulator::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer.pleaseStop(
        [this, completionHandler = move(completionHandler)]() mutable
        {
            m_mediatorUdpClient.reset();
            m_serverClient.reset();
            m_mediatorAddressPublisher.reset();
            m_stunPipeline.reset();
            m_udtStreamSocket.reset();
            m_udtStreamServerSocket.reset();

            completionHandler();
        });
}

}   //hpm
}   //nx
