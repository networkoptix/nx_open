#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <controller/connect_session_manager.h>

#include "connect_session_manager_mock.h"
#include "../basic_component_test.h"

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
    utils::SyncQueue<api::BeginListeningRequest> m_receivedBeginListeningRequests;
    utils::SyncQueue<api::CreateClientSessionRequest> m_receivedCreateClientSessionRequests;
    utils::SyncQueue<api::ConnectToPeerRequest> m_receivedConnectToPeerRequests;

    void thenRequestSucceeded()
    {
        ASSERT_EQ(api::ResultCode::ok, m_apiResponse.pop());
    }

    api::Client& relayClient()
    {
        if (!m_relayClient)
        {
            m_relayClient = api::ClientFactory::create(
                nx::network::url::Builder().setScheme("http").setHost("127.0.0.1")
                .setPort(moduleInstance()->httpEndpoints()[0].port));
        }
        return *m_relayClient;
    }

    void onRequestCompletion(api::ResultCode resultCode)
    {
        m_apiResponse.push(resultCode);
    }

private:
    controller::ConnectSessionManagerFactory::FactoryFunc m_factoryFuncBak;
    ConnectSessionManagerMock* m_connectSessionManager = nullptr;
    nx::utils::SyncQueue<api::ResultCode> m_apiResponse;
    std::unique_ptr<api::Client> m_relayClient;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_factoryFuncBak =
            controller::ConnectSessionManagerFactory::setFactoryFunc(
                std::bind(&HttpApi::createConnectSessionManager, this,
                    _1, _2, _3, _4));
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    virtual void TearDown() override
    {
        if (m_relayClient)
            m_relayClient->pleaseStopSync();
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
        auto connectSessionManager = 
            std::make_unique<ConnectSessionManagerMock>(
                &m_receivedBeginListeningRequests,
                &m_receivedCreateClientSessionRequests,
                &m_receivedConnectToPeerRequests);
        m_connectSessionManager = connectSessionManager.get();
        return std::move(connectSessionManager);
    }
};

//-------------------------------------------------------------------------------------------------
// HttpApiBeginListening

class HttpApiBeginListening:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().beginListening(
            "some_server_name",
            std::bind(&HttpApiBeginListening::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedBeginListeningRequests.pop();
    }
};

TEST_F(HttpApiBeginListening, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

//-------------------------------------------------------------------------------------------------
// HttpApiCreateClientSession

class HttpApiCreateClientSession:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().startSession(
            "some_session_id",
            "some_server_name",
            std::bind(&HttpApiCreateClientSession::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedCreateClientSessionRequests.pop();
    }
};

TEST_F(HttpApiCreateClientSession, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

//-------------------------------------------------------------------------------------------------
// HttpApiOpenConnectionToTheTargetHost

class HttpApiOpenConnectionToTheTargetHost:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().openConnectionToTheTargetHost(
            "some_session_id",
            std::bind(&HttpApiOpenConnectionToTheTargetHost::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedConnectToPeerRequests.pop();
    }
};

TEST_F(HttpApiOpenConnectionToTheTargetHost, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
