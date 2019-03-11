#include <gtest/gtest.h>

#include <nx/network/cloud/mediator_endpoint_provider.h>
#include <nx/network/cloud/mediator_stun_client.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/test_support/stun_simple_server.h>
#include <nx/network/test_support/stun_async_client_acceptance_tests.h>

namespace nx::hpm::api::test {

class MediatorStunClient:
    public nx::hpm::api::MediatorStunClient
{
    using base_type = nx::hpm::api::MediatorStunClient;

public:
    MediatorStunClient(AbstractAsyncClient::Settings settings):
        base_type(
            std::move(settings),
            /*MediatorEndpointProvider*/ nullptr)
    {
    }
};

//-------------------------------------------------------------------------------------------------

struct MediatorStunClientTestTypes
{
    using ClientType = MediatorStunClient;
    using ServerType = nx::network::stun::test::SimpleServer;
};

using namespace nx::network::stun::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    MediatorStunClient,
    StunAsyncClientAcceptanceTest,
    MediatorStunClientTestTypes);

//-------------------------------------------------------------------------------------------------

class MediatorStunClientKeepAlive:
    public StunAsyncClientAcceptanceTest<MediatorStunClientTestTypes>
{
public:
    MediatorStunClientKeepAlive()
    {
        enableProxy();

        nx::network::KeepAliveOptions keepAliveOptions;
        keepAliveOptions.inactivityPeriodBeforeFirstProbe = std::chrono::milliseconds(10);
        keepAliveOptions.probeSendPeriod = std::chrono::milliseconds(10);
        keepAliveOptions.probeCount = 3;
        client().setKeepAliveOptions(keepAliveOptions);
    }
};

TEST_F(MediatorStunClientKeepAlive, connection_is_closed_after_keep_alive_failure)
{
    givenConnectedClient();
    whenSilentlyDropAllConnectionsToServer();
    thenClientReportedConnectionClosureWithResult(SystemError::connectionReset);
}

//-------------------------------------------------------------------------------------------------

namespace {

class TestMediatorEndpointProvider:
    public AbstractMediatorEndpointProvider
{
public:
    virtual void fetchMediatorEndpoints(
        FetchMediatorEndpointsCompletionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                ++m_fetchAddressCallCount;

                handler(m_address
                    ? nx::network::http::StatusCode::ok
                    : nx::network::http::StatusCode::notFound);
            });
    }

    virtual std::optional<MediatorAddress> mediatorAddress() const override
    {
        return m_address;
    }

    void setMediatorAddress(const std::optional<MediatorAddress>& address)
    {
        executeInAioThreadSync([this, address]() { m_address = address; });
    }

    void waitForTheNextFetchAddressAttempt()
    {
        const int count = m_fetchAddressCallCount.load();
        while (m_fetchAddressCallCount.load() < count + 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    std::optional<MediatorAddress> m_address;
    std::atomic<int> m_fetchAddressCallCount{0};
};

} // namespace

//-------------------------------------------------------------------------------------------------

class MediatorStunClientWithEndpointProvider:
    public ::testing::Test
{
public:
    MediatorStunClientWithEndpointProvider()
    {
        network::stun::AbstractAsyncClient::Settings settings;
        settings.reconnectPolicy.initialDelay = std::chrono::milliseconds(100);

        m_client = std::make_unique<api::MediatorStunClient>(
            settings,
            &m_mediatorEndpointProvider);

        m_client->addOnReconnectedHandler([this]() { saveReconnect(); });
    }

    ~MediatorStunClientWithEndpointProvider()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        startServer();

        whenStartEndpointProvider();
    }

    void givenConnectedClient()
    {
        whenSendRequest();
        thenResponseIsReceived();
    }

    void whenSendRequest()
    {
        using namespace nx::network::stun;

        Message request(Header(MessageClass::request, bindingMethod));
        m_client->sendRequest(
            request,
            [this](auto&&... args) { saveRequestResult(std::forward<decltype(args)>(args)...); });
    }

    void whenStartEndpointProvider()
    {
        MediatorAddress address;
        address.tcpUrl = nx::network::url::Builder()
            .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(m_server->address());

        m_mediatorEndpointProvider.setMediatorAddress(address);
    }

    void whenStopEndpointProvider()
    {
        m_mediatorEndpointProvider.setMediatorAddress(std::nullopt);
    }

    void whenRestartServer()
    {
        stopServer();
        startServer();
    }

    void thenResponseIsReceived()
    {
        auto[resultCode, response] = m_requestResults.pop();
        ASSERT_EQ(SystemError::noError, resultCode);
    }

    void waitForReconnectAttemptToFail()
    {
        m_mediatorEndpointProvider.waitForTheNextFetchAddressAttempt();
    }

    void waitForConnectionEstablishedEvent()
    {
        m_reconnects.pop();
    }

private:
    using RequestResult = std::tuple<SystemError::ErrorCode, nx::network::stun::Message>;

    std::unique_ptr<nx::network::stun::test::SimpleServer> m_server;
    TestMediatorEndpointProvider m_mediatorEndpointProvider;
    std::unique_ptr<api::MediatorStunClient> m_client;
    nx::utils::SyncQueue<RequestResult> m_requestResults;
    nx::utils::SyncQueue</*dummy*/ int> m_reconnects;

    void startServer()
    {
        m_server = std::make_unique<nx::network::stun::test::SimpleServer>();
        ASSERT_TRUE(m_server->bind(nx::network::SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_server->listen());
    }

    void stopServer()
    {
        m_server->pleaseStopSync();
        m_server.reset();
    }

    void saveRequestResult(
        SystemError::ErrorCode resultCode,
        nx::network::stun::Message response)
    {
        m_requestResults.push(std::make_tuple(resultCode, std::move(response)));
    }

    void saveReconnect()
    {
        m_reconnects.push(/*dummy*/ 0);
    }
};

TEST_F(MediatorStunClientWithEndpointProvider, fetches_endpoint_from_endpoint_provider)
{
    whenSendRequest();
    thenResponseIsReceived();
}

TEST_F(MediatorStunClientWithEndpointProvider, reconnects_after_endpoint_provider_service_restart)
{
    givenConnectedClient();
    waitForConnectionEstablishedEvent();

    whenStopEndpointProvider();
    whenRestartServer();
    waitForReconnectAttemptToFail();
    whenStartEndpointProvider();

    waitForConnectionEstablishedEvent();

    whenSendRequest();
    thenResponseIsReceived();
}

} // namespace nx::hpm::api::test
