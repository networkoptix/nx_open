#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>
#include <nx/utils/sync_call.h>

namespace nx_http {
namespace server {
namespace test {

static const char* const kStunOverHttpPath = "/StunOverHttp";
static constexpr int kStunMethodToUse = nx::stun::MethodType::userMethod + 1;

/**
 * Tests tunneling of STUN protocol into HTTP connection.
 */
class HttpTunnelingServer:
    public ::testing::Test
{
public:
    HttpTunnelingServer():
        m_stunOverHttpServer(&m_dispatcher)
    {
    }

protected:
    void givenTunnelingServer()
    {
        m_stunOverHttpServer.setupHttpTunneling(
            &m_httpServer.httpMessageDispatcher(),
            kStunOverHttpPath);
    }

    void whenEstablishConnection()
    {
        using namespace std::placeholders;

        nx_http::HttpClient httpClient;
        ASSERT_TRUE(
            httpClient.doUpgrade(
                nx::network::url::Builder()
                    .setEndpoint(m_httpServer.serverAddress())
                    .setPath(kStunOverHttpPath).toUrl(),
                nx::stun::StunOverHttpServer::kStunProtocolName));
        ASSERT_EQ(
            nx_http::StatusCode::switchingProtocols,
            httpClient.response()->statusLine.statusCode);
        m_httpTunnelConnection = httpClient.takeSocket();
    }

    void whenSendStunRequest()
    {
        using namespace std::placeholders;

        nx::stun::AsyncClient stunClient(std::move(m_httpTunnelConnection));
        auto stunClientGuard = makeScopeGuard(
            [&stunClient]() { stunClient.pleaseStopSync(); });

        nx::stun::Message request(nx::stun::Header(
            nx::stun::MessageClass::request,
            kStunMethodToUse));

        auto result = makeSyncCall<SystemError::ErrorCode, nx::stun::Message>(
            std::bind(&nx::stun::AsyncClient::sendRequest, &stunClient,
                request, _1, nullptr));
        ASSERT_EQ(SystemError::noError, std::get<0>(result));
        ASSERT_EQ(
            nx::stun::MessageClass::successResponse,
            std::get<1>(result).header.messageClass);
    }

    void thenStunMessageIsReceivedByServer()
    {
        m_messagesReceived.pop();
    }

private:
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<nx::stun::Message> m_messagesReceived;
    nx::stun::MessageDispatcher m_dispatcher;
    nx::stun::StunOverHttpServer m_stunOverHttpServer;
    std::unique_ptr<AbstractStreamSocket> m_httpTunnelConnection;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_dispatcher.registerRequestProcessor(
            kStunMethodToUse,
            std::bind(&HttpTunnelingServer::processStunMessage, this, _1, _2));
    }

    void processStunMessage(
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
};

TEST_F(HttpTunnelingServer, tunnel_is_established)
{
    givenTunnelingServer();

    whenEstablishConnection();
    whenSendStunRequest();

    thenStunMessageIsReceivedByServer();
}

} // namespace test
} // namespace server
} // namespace nx_http
