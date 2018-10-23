#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/synchronous_tcp_server.h>

namespace nx {
namespace network {
namespace http {
namespace test {


// TODO: Use TestHttpServer?
class AuthHttpServer:
    public nx::network::test::SynchronousStreamSocketServer
{
public:
    struct AuthHeader {
        AuthType type;
        nx::Buffer header;
    };

    void appendAuthHeader(AuthHeader value);
    std::vector<nx::Buffer> receivedRequests();

private:
    enum class NextResponse {unauthorized, success};

    std::vector<nx::Buffer> m_receivedRequests;
    std::vector<AuthHeader> m_authHeaders;
    NextResponse m_nextRepsonse = NextResponse::unauthorized;

    virtual void processConnection(AbstractStreamSocket* connection) override;
    nx::Buffer nextResponse();
    void sendResponse(AbstractStreamSocket* connection);
    std::pair<int, nx::Buffer> readRequest(AbstractStreamSocket* connection);
};

struct TestParams
{
    std::vector<AuthHttpServer::AuthHeader> serverAuthHeaders;
    AuthType clientRequiredToUseAuthType;

    const char* expectedAuthResponse;
    int expectedHttpCode;
};

// TODO: Rename class, try to merge with another one.
class HttpClientAsyncAuthorization2:
    public ::testing::TestWithParam<TestParams>
{
public:
    HttpClientAsyncAuthorization2();
    virtual ~HttpClientAsyncAuthorization2() override;

    void givenHttpServerWithAuthorization(
        std::vector<AuthHttpServer::AuthHeader> authData);
    void whenClientSendHttpRequestAndIsRequiredToUse(AuthType auth);
    void thenClientAuthenticatedBy(const char* exptectedHeaderResponse);
    void thenClientGotResponseWithCode(int expectedHttpCode);

private:
    std::unique_ptr<http::AsyncClient> m_httpClient;
    std::unique_ptr<AuthHttpServer> m_httpServer;
};


} // namespace test
} // namespace nx
} // namespace network
} // namespace http
