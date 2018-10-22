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
    std::vector<nx::Buffer> receivedRequests();
    void enableAuthHeaders(std::optional<AuthType> first, std::optional<AuthType> second);

protected:
    virtual void processConnection(AbstractStreamSocket* connection) override;

private:
    enum class NextResponse {unauthorized, success};

    std::vector<nx::Buffer> m_receivedRequests;

    nx::Buffer m_firstAuthHeader;
    nx::Buffer m_secondAuthHeader;

    nx::Buffer nextResponse();
    void sendResponse(AbstractStreamSocket* connection);
    nx::Buffer readRequest(AbstractStreamSocket* connection);
};

struct TestParams
{
    std::optional<AuthType> serverAuthTypeFirst;
    std::optional<AuthType> serverAuthTypeSecond;
    AuthType clientRequiredToUseAuthType;

    AuthType expectedAuthType;
};

// TODO: Rename class, try to merge with another one.
class HttpClientAsyncAuthorization2:
    public ::testing::TestWithParam<TestParams>
{
public:
    HttpClientAsyncAuthorization2();
    virtual ~HttpClientAsyncAuthorization2() override;

    void givenHttpServerWithAuthorization(std::optional<AuthType> first,
        std::optional<AuthType> second);
    void whenClientSendHttpRequestAndIsRequiredToUse(AuthType auth);
    void thenClientAuthenticatedBy(AuthType auth);

private:
    std::unique_ptr<http::AsyncClient> m_httpClient;
    std::unique_ptr<AuthHttpServer> m_httpServer;
};


} // namespace test
} // namespace nx
} // namespace network
} // namespace http
