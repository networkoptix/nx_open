#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/synchronous_tcp_server.h>

namespace nx::network::http::test {

// TODO: Use TestHttpServer?
class AuthHttpServer:
    public nx::network::test::SynchronousStreamSocketServer
{
public:
    struct AuthHeader
    {
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
    AuthType clientRequiredToUseAuthType = AuthType::authBasicAndDigest;

    const char* expectedAuthResponse = nullptr;
    int expectedHttpCode = 0;
};

class HttpClientAsyncAuthorization:
    public ::testing::TestWithParam<TestParams>
{
public:
    HttpClientAsyncAuthorization();
    virtual ~HttpClientAsyncAuthorization() override;

    void givenHttpServerWithAuthorization(
        std::vector<AuthHttpServer::AuthHeader> authData);
    void whenClientSendHttpRequestAndIsRequiredToUse(AuthType auth, const char* username = nullptr);
    void thenClientAuthenticatedBy(const char* exptectedHeaderResponse);
    void thenClientGotResponseWithCode(int expectedHttpCode);
    void thenLastRequestAuthorizedOnServerAsUser(const std::string& username);

private:
    std::unique_ptr<http::AsyncClient> m_httpClient;
    std::unique_ptr<AuthHttpServer> m_httpServer;
};

} // namespace nx::network::http::test
