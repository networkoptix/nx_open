// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_listening_peer.h"

#include <nx/network/address_resolver.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_type.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/cloud/data/tunnel_connection_chosen_data.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/barrier_handler.h>

namespace nx::network::cloud::test {

using namespace nx::hpm;

template<typename Data>
struct VmsApiResult
{
    std::string error;
    std::string errorString;
    std::optional<Data> reply;
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(VmsApiResult, (error)(errorString)(reply))

struct VmsApiModuleInformation
{
    std::string realm;
    std::string cloudHost;
    nx::Uuid id;
    std::string cloudSystemId;
};

NX_REFLECTION_INSTRUMENT(VmsApiModuleInformation, (realm)(cloudHost)(id)(cloudSystemId))

//-------------------------------------------------------------------------------------------------

using namespace nx::network;

class ApiModuleInformationHandler:
    public nx::network::http::RequestHandlerWithContext
{
public:
    ApiModuleInformationHandler(
        std::optional<std::string> cloudSystemId,
        std::optional<std::string> serverIdForModuleInformation)
        :
        m_cloudSystemId(std::move(cloudSystemId)),
        m_serverIdForModuleInformation(std::move(serverIdForModuleInformation))
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler handler) override
    {
        VmsApiResult<VmsApiModuleInformation> restResult;
        if (!m_serverIdForModuleInformation)
        {
            restResult.error = "cantProcessRequest";
            restResult.errorString = "I am not a server";
        }
        else
        {
            restResult.error = "ok";
            restResult.reply = VmsApiModuleInformation();

            restResult.reply->realm = nx::network::AppInfo::realm();
            restResult.reply->cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
            restResult.reply->id = nx::Uuid::fromStringSafe(*m_serverIdForModuleInformation);
            if (m_cloudSystemId)
                restResult.reply->cloudSystemId = *m_cloudSystemId;
        }

        std::unique_ptr<nx::network::http::AbstractMsgBodySource> bodySource =
            std::make_unique<nx::network::http::BufferSource>(
                "application/json",
                nx::reflect::json::serialize(restResult));

        handler(nx::network::http::RequestResult{
            nx::network::http::StatusCode::ok,
            std::move(bodySource)});
    }

private:
    const std::optional<std::string> m_cloudSystemId;
    const std::optional<std::string> m_serverIdForModuleInformation;
};

//-------------------------------------------------------------------------------------------------

TestListeningPeer::TestListeningPeer(
    const network::SocketAddress& mediatorUdpEndpoint,
    const nx::utils::Url& mediatorTcpUrl,
    SystemCredentials systemData,
    std::string serverName)
:
    m_mediatorConnector(std::make_unique<hpm::api::MediatorConnector>("127.0.0.1")),
    m_httpServer(std::make_unique<nx::network::http::HttpStreamSocketServer>(
        &m_httpMessageDispatcher)),
    m_systemData(std::move(systemData)),
    m_serverId(
        serverName.empty()
        ? nx::Uuid::createUuid().toSimpleStdString()
        : std::move(serverName)),
    m_mediatorUdpEndpoint(mediatorUdpEndpoint),
    m_mediatorUdpClient(
        std::make_unique<nx::hpm::api::MediatorServerUdpConnection>(
            mediatorUdpEndpoint,
            m_mediatorConnector.get())),
    m_action(ActionToTake::proceedWithConnection),
    m_cloudConnectionMethodMask((int) network::cloud::ConnectType::udpHp),
    m_cloudSystemIdForModuleInformation(m_systemData.id),
    m_serverIdForModuleInformation(m_serverId)
{
    // Registering /api/moduleInformation handler.
    m_httpMessageDispatcher.registerRequestProcessor(
        "/api/moduleInformation",
        [this]()
        {
            return std::make_unique<ApiModuleInformationHandler>(
                m_cloudSystemIdForModuleInformation,
                m_serverIdForModuleInformation);
        });

    bindToAioThread(getAioThread());

    m_mediatorConnector->mockupMediatorAddress({
        mediatorTcpUrl,
        mediatorUdpEndpoint});

    m_mediatorConnector->setSystemCredentials(
        api::SystemCredentials(
            m_systemData.id,
            m_serverId,
            m_systemData.authKey));
}

TestListeningPeer::~TestListeningPeer()
{
    pleaseStopSync();
    m_httpServer.reset();
}

void TestListeningPeer::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    if (m_serverClient)
        m_serverClient->bindToAioThread(aioThread);
    if (m_mediatorAddressPublisher)
        m_mediatorAddressPublisher->bindToAioThread(aioThread);
    if (m_stunPipeline)
        m_stunPipeline->bindToAioThread(aioThread);
    if (m_udtStreamSocket)
        m_udtStreamSocket->bindToAioThread(aioThread);
    if (m_udtStreamServerSocket)
        m_udtStreamServerSocket->bindToAioThread(aioThread);
    if (m_httpServer)
        m_httpServer->bindToAioThread(aioThread);
    if (m_mediatorUdpClient)
        m_mediatorUdpClient->bindToAioThread(aioThread);
    if (m_mediatorConnector)
        m_mediatorConnector->bindToAioThread(aioThread);
}

bool TestListeningPeer::start(bool listenToConnectRequests)
{
    if (!m_httpServer->bind(network::SocketAddress(network::HostAddress::localhost, 0)) ||
        !m_httpServer->listen())
    {
        return false;
    }

    m_serverClient = m_mediatorConnector->systemConnection();
    m_serverClient->bindToAioThread(getAioThread());
    if (listenToConnectRequests)
    {
        m_serverClient->setOnConnectionRequestedHandler(
            [this](auto&&... args) { onConnectionRequested(std::forward<decltype(args)>(args)...); });
    }

    auto client = m_mediatorConnector->systemConnection();
    client->bindToAioThread(getAioThread());
    m_mediatorAddressPublisher = std::make_unique<network::cloud::MediatorAddressPublisher>(
        std::move(client),
        m_mediatorConnector.get());

    return true;
}

std::string TestListeningPeer::serverId() const
{
    return m_serverId;
}

void TestListeningPeer::setServerId(std::string serverId)
{
    m_serverId = serverId;
}

std::string TestListeningPeer::fullName() const
{
    return m_serverId + "." + m_systemData.id;
}

network::SocketAddress TestListeningPeer::endpoint() const
{
    return m_httpServer->address();
}

nx::hpm::api::ResultCode TestListeningPeer::bind()
{
    return updateTcpAddresses({endpoint()});
}

std::pair<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>
    TestListeningPeer::listen() const
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

network::SocketAddress TestListeningPeer::mediatorConnectionLocalAddress() const
{
    return m_serverClient->localAddress();
}

network::SocketAddress TestListeningPeer::udpHolePunchingEndpoint() const
{
    return m_mediatorUdpClient->localAddress();
}

void TestListeningPeer::setOnConnectionRequestedHandler(
    std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)> handler)
{
    m_onConnectionRequestedHandler = std::move(handler);
}

void TestListeningPeer::setConnectionAckResponseHandler(
    std::function<ActionToTake(api::ResultCode)> handler)
{
    m_connectionAckResponseHandler = std::move(handler);
}

void TestListeningPeer::setCloudSystemIdForModuleInformation(
    std::optional<std::string> cloudSystemId)
{
    m_cloudSystemIdForModuleInformation = cloudSystemId;
}

void TestListeningPeer::setServerIdForModuleInformation(
    std::optional<std::string> serverId)
{
    m_serverIdForModuleInformation = serverId;
}

nx::hpm::api::ResultCode TestListeningPeer::updateTcpAddresses(
    std::vector<network::SocketAddress> addresses)
{
    utils::promise<nx::hpm::api::ResultCode> promise;
    m_mediatorAddressPublisher->updateAddresses(
        std::move(addresses),
        [&promise](nx::hpm::api::ResultCode resultCode)
        {
            promise.set_value(resultCode);
        });

    return promise.get_future().get();
}

hpm::api::MediatorConnector& TestListeningPeer::mediatorConnector()
{
    return *m_mediatorConnector;
}

std::unique_ptr<hpm::api::MediatorServerTcpConnection> TestListeningPeer::mediatorConnection()
{
    return m_mediatorConnector->systemConnection();
}

void TestListeningPeer::setCloudConnectionMethodMask(int mask)
{
    m_cloudConnectionMethodMask = mask;
}

void TestListeningPeer::onConnectionRequested(
    nx::hpm::api::ConnectionRequestedEvent connectionRequestedData)
{
    using namespace std::placeholders;

    m_connectionRequestedData = connectionRequestedData;

    nx::hpm::api::ConnectionAckRequest connectionAckData;
    connectionAckData.connectSessionId = connectionRequestedData.connectSessionId;
    if ((m_cloudConnectionMethodMask & (int)network::cloud::ConnectType::udpHp) > 0)
        connectionAckData.connectionMethods = nx::hpm::api::ConnectionMethod::udpHolePunching;
    if ((m_cloudConnectionMethodMask & (int)network::cloud::ConnectType::proxy) > 0)
        connectionAckData.connectionMethods = nx::hpm::api::ConnectionMethod::proxy;

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

    if (!m_mediatorUdpClient)
    {
        m_mediatorUdpClient =
            std::make_unique<nx::hpm::api::MediatorServerUdpConnection>(
                m_mediatorConnector->address()->stunUdpEndpoint,
                m_mediatorConnector.get());
        m_mediatorUdpClient->bindToAioThread(getAioThread());
    }

    m_mediatorUdpClient->connectionAck(
        std::move(connectionAckData),
        std::bind(&TestListeningPeer::onConnectionAckResponseReceived, this, _1));
}

void TestListeningPeer::onConnectionAckResponseReceived(
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
    udtStreamSocket->bindToAioThread(getAioThread());
    auto mediatorUdpClientSocket = m_mediatorUdpClient->takeSocket();
    m_mediatorUdpClient.reset();
    if (!udtStreamSocket->bindToUdpSocket(mediatorUdpClientSocket.get()) ||
        !udtStreamSocket->setNonBlockingMode(true) ||
        !udtStreamSocket->setRendezvous(true))
    {
        return;
    }

    auto udtStreamServerSocket = std::make_unique<nx::network::UdtStreamServerSocket>(AF_INET);
    udtStreamServerSocket->bindToAioThread(getAioThread());
    if (!udtStreamServerSocket->setReuseAddrFlag(true) ||
        !udtStreamServerSocket->bind(udtStreamSocket->getLocalAddress()) ||
        !udtStreamServerSocket->setNonBlockingMode(true))
    {
        return;
    }

    m_udtStreamSocket = std::move(udtStreamSocket);
    m_udtStreamServerSocket = std::move(udtStreamServerSocket);

    NX_VERBOSE(this, "Starting rendezvous connect from %1 to %2",
        m_udtStreamSocket->getLocalAddress(), m_connectionRequestedData.udpEndpointList.front());

    m_udtStreamSocket->connectAsync(
        m_connectionRequestedData.udpEndpointList.front(),
        [this](auto&&... args) { onUdtConnectDone(std::forward<decltype(args)>(args)...); });

    m_udtStreamServerSocket->acceptAsync(
        [this](auto&&... args) { onUdtConnectionAccepted(std::forward<decltype(args)>(args)...); });
}

void TestListeningPeer::onUdtConnectDone(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
        return;

    m_stunPipeline = std::make_unique<network::stun::MessagePipeline>(
        std::move(m_udtStreamSocket));
    m_stunPipeline->bindToAioThread(getAioThread());
    m_udtStreamSocket.reset();

    m_stunPipeline->setMessageHandler(
        [this](auto&&... args) { onMessageReceived(std::forward<decltype(args)>(args)...); });
    m_stunPipeline->startReadingConnection();
}

void TestListeningPeer::onUdtConnectionAccepted(
    SystemError::ErrorCode /*errorCode*/,
    std::unique_ptr<network::AbstractStreamSocket> /*acceptedSocket*/)
{
}

void TestListeningPeer::onMessageReceived(
    network::stun::Message message)
{
    if (m_action <= ActionToTake::ignoreSyn)
        return;

    if (message.header.messageClass != network::stun::MessageClass::request)
        return;

    if (message.header.method == stun::extension::methods::udpHolePunchingSyn)
    {
        hpm::api::UdpHolePunchingSynResponse synAckResponse;
        if (m_action <= ActionToTake::sendBadSynAck)
            synAckResponse.connectSessionId = "4588809B-0210-49B0-87D9-44C5200F2062";
        else
            synAckResponse.connectSessionId = m_connectionRequestedData.connectSessionId;
        stun::Message synAckMessage(
            nx::network::stun::Header(
                nx::network::stun::MessageClass::successResponse,
                hpm::api::UdpHolePunchingSynResponse::kMethod,
                message.header.transactionId));
        synAckResponse.serialize(&synAckMessage);
        m_stunPipeline->sendMessage(std::move(synAckMessage));
    }
    else if (message.header.method == stun::extension::methods::tunnelConnectionChosen)
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

void TestListeningPeer::stopWhileInAioThread()
{
    m_mediatorUdpClient.reset();
    m_serverClient.reset();
    m_mediatorAddressPublisher.reset();
    m_stunPipeline.reset();
    m_udtStreamSocket.reset();
    m_udtStreamServerSocket.reset();

    // NOTE: m_httpServer does not support non-blocking destruction
    m_httpServer->pleaseStopSync();
    m_mediatorConnector.reset();
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<TestListeningPeer> TestListeningPeer::buildServer(
    const SystemCredentials& system,
    std::string name,
    ServerTweak serverConf,
    const SocketAddress& stunUdpEndpoint,
    const nx::utils::Url& mediatorTcpUrl)
{
    auto server = std::make_unique<TestListeningPeer>(
        stunUdpEndpoint,
        mediatorTcpUrl,
        system,
        std::move(name));

    if (!server->start(serverConf.listenToConnect))
    {
        NX_ERROR(typeid(TestListeningPeer), "Failed to start server: %1", server->fullName());
        return nullptr;
    }

    if (serverConf.bindEndpoint)
    {
        if (const auto rc = server->bind(); rc != nx::hpm::api::ResultCode::ok)
        {
            NX_ERROR(typeid(TestListeningPeer), "Failed to bind server: %1, endpoint=%2: %3",
                server->fullName(), server->endpoint(), rc);
            return nullptr;
        }
    }

    return server;
}

} // namespace nx::network::cloud::test
