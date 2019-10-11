#include "../tunneling_acceptance_tests.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/detail/request_paths.h>
#include <nx/utils/scope_guard.h>

namespace nx::network::http::tunneling::test {

struct ExperimentalMethodMask { static constexpr int value = TunnelMethod::experimental; };

class SeparateUpDownConnectionsTunnel:
    public HttpTunneling<ExperimentalMethodMask>
{
    using base_type = HttpTunneling<ExperimentalMethodMask>;

public:
    ~SeparateUpDownConnectionsTunnel()
    {
        for (;;)
        {
            auto httpClient = m_completedOpenDownChannelRequests.pop(
                std::chrono::milliseconds::zero());
            if (!httpClient || !*httpClient)
                break;
            (*httpClient)->pleaseStopSync();
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnellingServer();
    }

    void whenOpenDownChannel()
    {
        auto httpClient = std::make_unique<AsyncClient>();
        auto httpClientPtr = httpClient.get();

        httpClientPtr->setOnResponseReceived(
            [this, httpClient = std::move(httpClient)]() mutable
            {
                saveDownChannelConnection(std::move(httpClient));
            });

        httpClientPtr->doGet(
            downChannelUrl(),
            [this]() { saveDownChannelConnection(nullptr); });
    }

    void thenDownChannelIsOpened()
    {
        auto httpClient = m_completedOpenDownChannelRequests.pop();
        ASSERT_NE(nullptr, httpClient);

        auto httpClientGuard = nx::utils::makeScopeGuard(
            [&httpClient]() { httpClient->pleaseStopSync(); });

        ASSERT_FALSE(httpClient->failed());
        ASSERT_EQ(StatusCode::ok, httpClient->response()->statusLine.statusCode);
    }

    void waitUntilServerClosesConnectionGracefully()
    {
        ASSERT_TRUE(m_downChannelConnection->setNonBlockingMode(false)
            && m_downChannelConnection->setRecvTimeout(kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();

        for (;;)
        {
            char buf[128];
            auto bytesRead = m_downChannelConnection->recv(buf, sizeof(buf));
            ASSERT_GE(bytesRead, 0) << SystemError::getLastOSErrorText().toStdString();
            if (bytesRead == 0)
                break;
        }
    }

private:
    nx::utils::SyncQueue<std::unique_ptr<AsyncClient>> m_completedOpenDownChannelRequests;
    std::unique_ptr<AbstractStreamSocket> m_downChannelConnection;

    nx::utils::Url downChannelUrl()
    {
        return nx::network::url::Builder(baseUrl()).appendPath(
            rest::substituteParameters(detail::kExperimentalTunnelDownPath, {"testTunnelId"}))
            .toUrl();
    }

    void saveDownChannelConnection(std::unique_ptr<AsyncClient> httpClient)
    {
        if (httpClient)
            m_downChannelConnection = httpClient->takeSocket();
        m_completedOpenDownChannelRequests.push(std::move(httpClient));
    }
};

TEST_F(SeparateUpDownConnectionsTunnel, incomplete_tunnel_is_closed_by_timeout)
{
    setHttpConnectionInactivityTimeout(std::chrono::milliseconds(10));

    whenOpenDownChannel();
    thenDownChannelIsOpened();

    waitUntilServerClosesConnectionGracefully();
}

} // namespace nx::network::http::tunneling::test
