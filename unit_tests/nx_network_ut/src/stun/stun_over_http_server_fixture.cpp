#include "stun_over_http_server_fixture.h"

#include <nx/network/url/url_builder.h>

namespace nx {
namespace stun {
namespace test {

static const char* const kStunOverHttpPath = "/StunOverHttp";

StunOverHttpServer::StunOverHttpServer():
    m_stunOverHttpServer(&m_dispatcher)
{
    using namespace std::placeholders;

    m_stunOverHttpServer.setupHttpTunneling(
        &m_httpServer.httpMessageDispatcher(),
        kStunOverHttpPath);
}

bool StunOverHttpServer::bind(const SocketAddress& localEndpoint)
{
    return m_httpServer.bindAndListen(localEndpoint);
}

bool StunOverHttpServer::listen()
{
    return true;
}

nx::utils::Url StunOverHttpServer::getServerUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_httpServer.serverAddress())
        .setPath(kStunOverHttpPath).toUrl();
}

nx::network::stun::MessageDispatcher& StunOverHttpServer::dispatcher()
{
    return m_dispatcher;
}

void StunOverHttpServer::sendIndicationThroughEveryConnection(
    nx::network::stun::Message message)
{
    m_stunOverHttpServer.stunConnectionPool().forEachConnection(
        nx::network::server::MessageSender<nx::network::stun::ServerConnection>(std::move(message)));
}

std::size_t StunOverHttpServer::connectionCount() const
{
    return m_stunOverHttpServer.stunConnectionPool().connectionCount();
}

//-------------------------------------------------------------------------------------------------

static constexpr int kStunMethodToUse = nx::network::stun::MethodType::userMethod + 1;

void StunOverHttpServerFixture::SetUp()
{
    using namespace std::placeholders;

    m_server.dispatcher().registerRequestProcessor(
        kStunMethodToUse,
        std::bind(&StunOverHttpServerFixture::processStunMessage, this, _1, _2));

    ASSERT_TRUE(m_server.bind(SocketAddress::anyPrivateAddress));
    ASSERT_TRUE(m_server.listen());
}

nx::network::stun::MessageDispatcher& StunOverHttpServerFixture::dispatcher()
{
    return m_server.dispatcher();
}

nx::utils::Url StunOverHttpServerFixture::tunnelUrl() const
{
    return m_server.getServerUrl();
}

nx::network::stun::Message StunOverHttpServerFixture::popReceivedMessage()
{
    return m_messagesReceived.pop();
}

nx::network::stun::Message StunOverHttpServerFixture::prepareRequest()
{
    return nx::network::stun::Message(
        nx::network::stun::Header(
            nx::network::stun::MessageClass::request,
            kStunMethodToUse));
}

void StunOverHttpServerFixture::givenTunnelingServer()
{
}

void StunOverHttpServerFixture::assertStunClientIsAbleToPerformRequest(
    AbstractAsyncClient* client)
{
    nx::utils::promise<
        std::tuple<SystemError::ErrorCode, nx::network::stun::Message>
    > responseReceived;

    client->sendRequest(
        prepareRequest(),
        [&responseReceived](
            SystemError::ErrorCode sysErrorCode,
            nx::network::stun::Message response)
        {
            responseReceived.set_value(
                std::make_tuple(sysErrorCode, std::move(response)));
        });

    auto result = responseReceived.get_future().get();
    ASSERT_EQ(SystemError::noError, std::get<0>(result));
    ASSERT_EQ(
        nx::network::stun::MessageClass::successResponse,
        std::get<1>(result).header.messageClass);
}

void StunOverHttpServerFixture::processStunMessage(
    std::shared_ptr<nx::network::stun::AbstractServerConnection> serverConnection,
    nx::network::stun::Message message)
{
    m_messagesReceived.push(message);

    nx::network::stun::Message response(nx::network::stun::Header(
        nx::network::stun::MessageClass::successResponse,
        message.header.method,
        message.header.transactionId));
    serverConnection->sendMessage(std::move(response), nullptr);
}

} // namespace test
} // namespace stun
} // namespace nx
