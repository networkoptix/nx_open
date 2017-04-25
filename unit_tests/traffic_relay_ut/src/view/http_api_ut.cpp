#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <controller/connect_session_manager.h>

#include "connect_session_manager_mock.h"
#include "../traffic_relay_basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class HttpApi:
    public ::testing::Test,
    public BasicComponentTest
{
public:

protected:
    void whenIssuedApiRequest()
    {
        m_httpClient = std::make_unique<nx_http::AsyncClient>();
        m_httpClient->doGet(
            prepareUrl(),
            std::bind(&HttpApi::onRequestCompletion, this));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        // TODO
    }

    void thenResponseHasBeenReceived()
    {
        m_apiResponse.pop();
    }

private:
    controller::ConnectSessionManagerFactory::FactoryFunc m_factoryFuncBak;
    ConnectSessionManagerMock* m_connectSessionManager = nullptr;
    nx::utils::SyncQueue<bool> m_apiResponse;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_factoryFuncBak =
            controller::ConnectSessionManagerFactory::setFactoryFunc(
                std::bind(&HttpApi::createConnectSessionManager, this, _1, _2, _3, _4));
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    virtual void TearDown() override
    {
        m_httpClient->pleaseStopSync();
        stop();

        controller::ConnectSessionManagerFactory::setFactoryFunc(
            std::move(m_factoryFuncBak));
    }

    std::unique_ptr<controller::AbstractConnectSessionManager> 
        createConnectSessionManager(
            const conf::Settings& /*settings*/,
            model::ClientSessionPool* /*clientSessionPool*/,
            model::ListeningPeerPool* /*listeningPeerPool*/,
            controller::AbstractTrafficRelay* /*trafficRelay*/)
    {
        auto connectSessionManager = std::make_unique<ConnectSessionManagerMock>();
        m_connectSessionManager = connectSessionManager.get();
        return connectSessionManager;
    }

    QUrl prepareUrl() const
    {
        return QUrl(lm("http://127.0.0.1:%1%2")
            .str(moduleInstance()->httpEndpoints()[0].port)
            .str(api::path::kServerIncomingConnections));
    }

    void onRequestCompletion()
    {
        m_apiResponse.push(true);
    }
};

TEST_F(HttpApi, test)
{
    whenIssuedApiRequest();
    thenRequestHasBeenDeliveredToTheManager();
    thenResponseHasBeenReceived();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
