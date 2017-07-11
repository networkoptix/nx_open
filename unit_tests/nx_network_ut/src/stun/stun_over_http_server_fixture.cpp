#include "stun_over_http_server_fixture.h"

#include <nx/network/url/url_builder.h>

namespace nx {
namespace stun {
namespace test {

static const char* const kStunOverHttpPath = "/StunOverHttp";
static constexpr int kStunMethodToUse = nx::stun::MethodType::userMethod + 1;

StunOverHttpServerFixture::StunOverHttpServerFixture():
    m_stunOverHttpServer(&m_dispatcher)
{
}

void StunOverHttpServerFixture::SetUp()
{
    using namespace std::placeholders;

    ASSERT_TRUE(m_httpServer.bindAndListen());

    m_dispatcher.registerRequestProcessor(
        kStunMethodToUse,
        std::bind(&StunOverHttpServerFixture::processStunMessage, this, _1, _2));
}

QUrl StunOverHttpServerFixture::tunnelUrl() const
{
    return nx::network::url::Builder()
        .setScheme("http")
        .setEndpoint(m_httpServer.serverAddress())
        .setPath(kStunOverHttpPath).toUrl();
}

nx::stun::Message StunOverHttpServerFixture::popReceivedMessage()
{
    return m_messagesReceived.pop();
}

nx::stun::Message StunOverHttpServerFixture::prepareRequest()
{
    return nx::stun::Message(
        nx::stun::Header(
            nx::stun::MessageClass::request,
            kStunMethodToUse));
}

void StunOverHttpServerFixture::givenTunnelingServer()
{
    m_stunOverHttpServer.setupHttpTunneling(
        &m_httpServer.httpMessageDispatcher(),
        kStunOverHttpPath);
}

void StunOverHttpServerFixture::assertStunClientIsAbleToPerformRequest(
    AbstractAsyncClient* client)
{
    nx::utils::promise<
        std::tuple<SystemError::ErrorCode, nx::stun::Message>
    > responseReceived;

    client->sendRequest(
        prepareRequest(),
        [&responseReceived](
            SystemError::ErrorCode sysErrorCode,
            nx::stun::Message response)
        {
            responseReceived.set_value(
                std::make_tuple(sysErrorCode, std::move(response)));
        });

    auto result = responseReceived.get_future().get();
    ASSERT_EQ(SystemError::noError, std::get<0>(result));
    ASSERT_EQ(
        nx::stun::MessageClass::successResponse,
        std::get<1>(result).header.messageClass);
}

void StunOverHttpServerFixture::processStunMessage(
    std::shared_ptr<nx::stun::AbstractServerConnection> serverConnection,
    nx::stun::Message message)
{
    m_messagesReceived.push(message);

    nx::stun::Message response(nx::stun::Header(
        nx::stun::MessageClass::successResponse,
        message.header.method,
        message.header.transactionId));
    serverConnection->sendMessage(std::move(response), nullptr);
}

} // namespace test
} // namespace stun
} // namespace nx
