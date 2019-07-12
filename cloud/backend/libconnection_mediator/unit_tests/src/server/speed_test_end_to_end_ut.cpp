#include "../functional_tests/mediator_functional_test.h"

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/speed_test/abstract_speed_tester.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester_factory.h>
#include <nx/network/cloud/speed_test/uplink_speed_reporter.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_connection.h"

namespace nx::hpm::test {

using namespace nx::network::cloud::speed_test;

class SpeedTestEndToEnd;

class UplinkSpeedTesterStub:
    public AbstractSpeedTester
{
public:
    UplinkSpeedTesterStub(SpeedTestEndToEnd* testFixture);
    ~UplinkSpeedTesterStub();

    virtual void start(const nx::utils::Url& speedTestUrl, CompletionHandler handler) override;

private:
    SpeedTestEndToEnd* m_testFixture = nullptr;
};

class SpeedTestEndToEnd:
    public MediatorFunctionalTest
{
private:
    struct TestContext
    {
        std::unique_ptr<MediaServerEmulator> mediaserver;
        nx::utils::Url speedTestUrl;
        api::PeerConnectionSpeed uplinkSpeed;
        std::unique_ptr<UplinkSpeedReporter> uplinkSpeedReporter;
    };

public:
    SpeedTestEndToEnd()
    {
        addArg("-listeningPeerDb/enabled", "true");
        addArg("-listeningPeerDb/enableCache", "true");
        addArg("-listeningPeerDb/sql/driverName", "QSQLITE");
        addArg("-server/name", "speedtestendtoend");
    }

    ~SpeedTestEndToEnd()
    {
        if (m_factoryFuncBak)
            UplinkSpeedTesterFactory::instance().setCustomFunc(std::move(m_factoryFuncBak));

        moduleInstance()->impl()->listeningPeerDb()
            .unsubscribeFromUplinkSpeedUpdated(m_uplinkSpeedUpdatedId);

        stop();
    }

    api::ConnectionSpeed getUplinkSpeed(const nx::utils::Url& url)
    {
        return std::find_if(m_servers.begin(), m_servers.end(),
            [this, &url](const TestContext& testContext)
            {
                return testContext.speedTestUrl == url;
            })->uplinkSpeed.connectionSpeed;
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_uplinkSpeedUpdatedId = moduleInstance()->impl()->listeningPeerDb().
            subscribeToUplinkSpeedUpdated([this](api::PeerConnectionSpeed peerUplinkSpeed)
                {
                    ++m_uplinkSpeedTestsDone;
                    NX_DEBUG(this, "Received peerConnectionSpeed for server: %1",
                        peerUplinkSpeed.serverId);
                });

        m_factoryFuncBak =
            UplinkSpeedTesterFactory::instance().setCustomFunc(
                [this]()
                {
                    return std::make_unique<UplinkSpeedTesterStub>(this);
                });
    }

    void givenMediator()
    {
        // Done in Setup()
    }

    void givenMediaservers()
    {
        for (int i = 0; i < m_serverCount; ++i)
            addServer();

        updateBestConnection();
    }

    void whenResultsAreReportedToMediator()
    {
        // speed tests are started and reported automatically by UplinkSpeedReporter
        // in it's constructor
        while (m_uplinkSpeedTestsDone != (int) m_serverCount)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void whenClientRequestToConnectToSystem()
    {
        m_client = std::make_unique<api::Client>(httpUrl());

        api::ConnectRequest request;
        request.connectionMethods = api::ConnectionMethod::all;
        request.destinationHostName = m_system->id;
        request.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();
        request.connectSessionId = QnUuid::createUuid().toSimpleByteArray();

        m_client->initiateConnection(
            request,
            [this](auto&&... args)
            {
                m_connectDone.push({std::forward<decltype(args)>(args)...});
            });
    }

    void thenClientConnectsToServerWithBestUplinkSpeed()
    {
        auto [resultCode, connectResponse] = m_connectDone.pop();
        ASSERT_EQ(api::Client::ResultCode::ok, resultCode);
        ASSERT_EQ(
            m_bestConnection->mediaserver->fullName(),
            connectResponse.destinationHostFullName);
    }

private:
    void addServer()
    {
        if (!m_system)
            m_system = addRandomSystem();

        m_servers.emplace_back(TestContext());
        m_servers.back().mediaserver =
            addRandomServer(
                *m_system,
                boost::none,
                ServerTweak::defaultBehavior,
                network::http::kUrlSchemeName);

        m_servers.back().uplinkSpeed.serverId = m_servers.back().mediaserver->serverId();
        m_servers.back().uplinkSpeed.systemId = m_system->id;
        m_servers.back().uplinkSpeed.connectionSpeed.bandwidth =
            nx::utils::random::number(10, 10000);
        m_servers.back().uplinkSpeed.connectionSpeed.pingTime =
            std::chrono::microseconds(nx::utils::random::number(10, 10000));

        m_servers.back().speedTestUrl = lm("http://speedtest.%1.com").arg(m_servers.size() - 1);

        ASSERT_EQ(api::ResultCode::ok, m_servers.back().mediaserver->listen().first);

        m_servers.back().uplinkSpeedReporter = std::make_unique<UplinkSpeedReporter>(
            &m_servers.back().mediaserver->mediatorConnector());
        m_servers.back().uplinkSpeedReporter->mockupSpeedTestUrl(m_servers.back().speedTestUrl);

        NX_DEBUG(this, "Added a new server: %1, server count: %2",
            m_servers.back().mediaserver->fullName(), m_servers.size());
    }

    void updateBestConnection()
    {
        m_bestConnection = &*std::max_element(m_servers.begin(), m_servers.end(),
            [this](const TestContext& a, const TestContext& b)
            {
                return a.uplinkSpeed.connectionSpeed.bandwidth
                    < b.uplinkSpeed.connectionSpeed.bandwidth;
            });
    }

private:
    UplinkSpeedTesterFactory::Function m_factoryFuncBak;
    std::optional<AbstractCloudDataProvider::System> m_system;
    int m_serverCount = 10;
    std::vector<TestContext> m_servers;
    std::atomic_int m_uplinkSpeedTestsDone = 0;
    nx::utils::SubscriptionId m_uplinkSpeedUpdatedId = nx::utils::kInvalidSubscriptionId;
    const TestContext* m_bestConnection = nullptr;
    nx::utils::SyncQueue<std::pair<api::Client::ResultCode, api::ConnectResponse>> m_connectDone;
    std::unique_ptr<api::Client> m_client;
};

//-------------------------------------------------------------------------------------------------
// UplinkSpeedTesterStub

UplinkSpeedTesterStub::UplinkSpeedTesterStub(SpeedTestEndToEnd* testFixture):
    m_testFixture(testFixture)
{
}

UplinkSpeedTesterStub::~UplinkSpeedTesterStub()
{
    pleaseStopSync();
}

void UplinkSpeedTesterStub::start(const nx::utils::Url& speedTestUrl, CompletionHandler handler)
{
    dispatch(
        [this, speedTestUrl, handler = std::move(handler)]()
        {
            handler(SystemError::noError, m_testFixture->getUplinkSpeed(speedTestUrl));
        });
}

//-------------------------------------------------------------------------------------------------

TEST_F(SpeedTestEndToEnd, client_connects_to_mediaserver_with_best_uplink_speed)
{
    givenMediator();
    givenMediaservers();

    whenResultsAreReportedToMediator();

    whenClientRequestToConnectToSystem();
    thenClientConnectsToServerWithBestUplinkSpeed();
}

} // namespace nx::hpm::test
