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
        QnMutexLocker lock(&m_mutex);
        return std::find_if(m_servers.begin(), m_servers.end(),
            [&url](const TestContext& testContext)
            {
                return testContext.speedTestUrl == url;
            })->uplinkSpeed.connectionSpeed;
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_uplinkSpeedUpdatedId = moduleInstance()->impl()->listeningPeerDb().
            subscribeToUplinkSpeedUpdated(
                [this](api::PeerConnectionSpeed peerUplinkSpeed)
                {
                    ++m_uplinkSpeedTestsDone;
                    NX_DEBUG(this, "Received peerUplinkSpeed: %1", peerUplinkSpeed);
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
		for (int i = 0; i < 5; ++i)
		{
			m_systems.emplace_back(addRandomSystem());
			for(int j = 0; j < 5; ++j)
				addServer(m_systems.back());
		}

        updateBestConnection();
    }

    void whenSpeedTestResultsAreReportedToMediator()
    {
        // Speed tests are started and reported automatically by UplinkSpeedReporter
        // in it's constructor, so just wait for them to be report.
		while (m_uplinkSpeedTestsDone != (int) m_servers.size())
		{
			NX_DEBUG(this, "%1: sleeping", __func__);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
    }

    void whenClientRequestToConnectToSystem()
    {
        m_client = std::make_unique<api::Client>(httpUrl());

		m_expectedSystem = &m_systems.front();

        api::ConnectRequest request;
        request.connectionMethods = api::ConnectionMethod::all;
        request.destinationHostName = m_expectedSystem->id;
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

	void andServerBelongsToExpectedSystem()
	{
		const QByteArray systemId = m_bestConnection->uplinkSpeed.systemId.c_str();
		ASSERT_EQ(systemId, m_expectedSystem->id);
	}

private:
    void addServer(const AbstractCloudDataProvider::System& system)
    {
        const auto keepAssigningBandwidth =
            [this](int bandwidth)
        {
            return std::find_if(m_servers.begin(), m_servers.end(),
                [&bandwidth](const TestContext& testContext)
                {
                    return bandwidth == testContext.uplinkSpeed.connectionSpeed.bandwidth;
                }) != m_servers.end();
        };

        {
            auto mediaServer = addRandomServer(
				system,
                boost::none,
                ServerTweak::defaultBehavior,
                network::http::kUrlSchemeName);
            ASSERT_NE(nullptr, mediaServer);

            QnMutexLocker lock(&m_mutex);

            m_servers.emplace_back(TestContext());
            m_servers.back().mediaserver = std::move(mediaServer);

            m_servers.back().uplinkSpeed.serverId =
                nx::toStdString(m_servers.back().mediaserver->serverId());
            m_servers.back().uplinkSpeed.systemId = nx::toStdString(system.id);

            // Ensuring every bandwidth is unique.
            int bandwidth = nx::utils::random::number(10, 10000);
            while (keepAssigningBandwidth(bandwidth))
                bandwidth = nx::utils::random::number(10, 10000);

            m_servers.back().uplinkSpeed.connectionSpeed.bandwidth = bandwidth;
            m_servers.back().uplinkSpeed.connectionSpeed.pingTime =
                std::chrono::microseconds(nx::utils::random::number(10, 10000));

            m_servers.back().speedTestUrl = lm("http://speedtest.%1.com").arg(m_servers.size() - 1);
        }

        ASSERT_EQ(api::ResultCode::ok, m_servers.back().mediaserver->listen().first);

        m_servers.back().uplinkSpeedReporter = std::make_unique<UplinkSpeedReporter>(
            &m_servers.back().mediaserver->mediatorConnector(),
            m_servers.back().speedTestUrl);

        NX_DEBUG(this, "Added a new server, index: %1, uplinkSpeed: %3",
            m_servers.size() - 1, m_servers.back().uplinkSpeed);
    }

    void updateBestConnection()
    {
        m_bestConnection = &*std::max_element(m_servers.begin(), m_servers.end(),
            [](const TestContext& a, const TestContext& b)
            {
                return a.uplinkSpeed.connectionSpeed.bandwidth
                    < b.uplinkSpeed.connectionSpeed.bandwidth;
            });
        NX_DEBUG(this, "Decided best uplink speed: %1", m_bestConnection->uplinkSpeed);
    }

private:
    UplinkSpeedTesterFactory::Function m_factoryFuncBak;
    QnMutex m_mutex;
	std::vector<AbstractCloudDataProvider::System> m_systems;
	AbstractCloudDataProvider::System* m_expectedSystem = nullptr;
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

    whenSpeedTestResultsAreReportedToMediator();
    whenClientRequestToConnectToSystem();

    thenClientConnectsToServerWithBestUplinkSpeed();

	andServerBelongsToExpectedSystem();
}

} // namespace nx::hpm::test
