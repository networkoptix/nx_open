#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/cloud/random_online_endpoint_selector.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

static constexpr int kTestHostCount = 4;

class RandomOnlineEndpointSelector:
    public ::testing::Test
{
public:
    RandomOnlineEndpointSelector()
    {
        for (int i = 0; i < kTestHostCount; ++i)
        {
            m_testEndpoints.push_back(
                nx::network::SocketAddress(
                    QnUuid::createUuid().toString() + ".ru", 80));
        }
    }

protected:
    void givenOnlyOfflineHosts()
    {
    }

    void givenOnlineHost()
    {
        ASSERT_TRUE(m_testHttpServer.bindAndListen());

        nx::network::SocketAddress& onlineHostEndpoint =
            m_testEndpoints[nx::utils::random::number<size_t>(0, m_testEndpoints.size()-1)];
        onlineHostEndpoint = m_testHttpServer.serverAddress();
        m_onlineHostEndpoint = onlineHostEndpoint;
    }

    void whenSelectorIsInvoked()
    {
        nx::utils::promise<std::pair<nx::network::http::StatusCode::Value, nx::network::SocketAddress>> promiseToSelect;
        nx::network::cloud::RandomOnlineEndpointSelector selector;

        auto selected = promiseToSelect.get_future();

        selector.selectBestEndpont(
            QString(),
            m_testEndpoints,
            [&promiseToSelect](
                nx::network::http::StatusCode::Value result, nx::network::SocketAddress endpoint)
            {
                promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
            });

        m_selectResult = selected.get();
    }

    void thenAnyOnlineHostHasBeenSelected()
    {
        ASSERT_EQ(nx::network::http::StatusCode::ok, m_selectResult.first);
        ASSERT_EQ(m_onlineHostEndpoint, m_selectResult.second);
    }

    void thenNoHostHasBeenSelected()
    {
        ASSERT_NE(nx::network::http::StatusCode::ok, m_selectResult.first);
    }

    const std::vector<nx::network::SocketAddress>& testEndpoints() const
    {
        return m_testEndpoints;
    }

private:
    std::vector<nx::network::SocketAddress> m_testEndpoints;
    nx::network::http::TestHttpServer m_testHttpServer;
    nx::network::SocketAddress m_onlineHostEndpoint;
    std::pair<nx::network::http::StatusCode::Value, nx::network::SocketAddress> m_selectResult;
};

TEST_F(RandomOnlineEndpointSelector, online_endpoint_is_selected)
{
    givenOnlineHost();
    whenSelectorIsInvoked();
    thenAnyOnlineHostHasBeenSelected();
}

TEST_F(RandomOnlineEndpointSelector, no_online_endpoint_to_select)
{
    givenOnlyOfflineHosts();
    whenSelectorIsInvoked();
    thenNoHostHasBeenSelected();
}

TEST_F(RandomOnlineEndpointSelector, cancellation_of_select)
{
    for (int i = 0; i < 20; ++i)
    {
        nx::utils::promise<std::pair<nx::network::http::StatusCode::Value, nx::network::SocketAddress>> promiseToSelect;

        nx::network::cloud::RandomOnlineEndpointSelector selector;
        auto selected = promiseToSelect.get_future();

        selector.selectBestEndpont(
            "",
            testEndpoints(),
            [&promiseToSelect](
                nx::network::http::StatusCode::Value result, nx::network::SocketAddress endpoint)
            {
                promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
            });
    }
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
