#include "tunneling_acceptance_tests.h"

#include <memory>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/detail/request_paths.h>

namespace nx::network::http::tunneling::test {

class HttpTunnelingGetPost:
    public HttpTunneling
{
public:
    HttpTunnelingGetPost()
    {
        setHttpConnectionInactivityTimeout(std::chrono::milliseconds(100));
    }

    ~HttpTunnelingGetPost()
    {
        if (m_httpClient)
            m_httpClient->pleaseStopSync();

        if (m_tunnelConnection)
            m_tunnelConnection->pleaseStopSync();
    }

protected:
    void whenSendGetRequest()
    {
        const auto url = nx::network::url::Builder(baseUrl())
            .appendPath(rest::substituteParameters(detail::kGetPostTunnelPath, {"1"})).toUrl();

        m_httpClient = std::make_unique<AsyncClient>();
        std::promise<StatusCode::Value> done;
        m_httpClient->doGet(
            url,
            [this, &done]()
            {
                m_tunnelConnection = m_httpClient->takeSocket();
                done.set_value((StatusCode::Value) m_httpClient->response()->statusLine.statusCode);
            });
        ASSERT_EQ(StatusCode::ok, done.get_future().get());
    }

    void thenTunnelIsClosedByServerEventually()
    {
        ASSERT_TRUE(m_tunnelConnection->setNonBlockingMode(false));
        ASSERT_TRUE(m_tunnelConnection->setRecvTimeout(kNoTimeout));

        for (;;)
        {
            char buf[128];
            const auto bytesReceived = m_tunnelConnection->recv(buf, sizeof(buf));
            if (bytesReceived < 0 && SystemError::getLastOSErrorCode() == SystemError::interrupted)
                continue;

            ASSERT_EQ(0, bytesReceived);
            break;
        }
    }

private:
    std::unique_ptr<AsyncClient> m_httpClient;
    std::unique_ptr<AbstractStreamSocket> m_tunnelConnection;
};

INSTANTIATE_TEST_CASE_P(
    Specific,
    HttpTunnelingGetPost,
    ::testing::Values(TunnelMethod::getPost));

TEST_P(HttpTunnelingGetPost, connection_is_closed_by_timeout_if_post_never_comes)
{
    whenSendGetRequest();
    thenTunnelIsClosedByServerEventually();
}

} // namespace nx::network::http::tunneling::test
