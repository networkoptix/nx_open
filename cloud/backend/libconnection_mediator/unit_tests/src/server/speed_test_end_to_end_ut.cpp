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
			[&url](const auto& element)
			{
				return element.second.speedTestUrl == url;
			})->second.uplinkSpeed.connectionSpeed;
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        moduleInstance()->impl()->listeningPeerDb().
            subscribeToUplinkSpeedUpdated(
                [this](api::PeerConnectionSpeed peerUplinkSpeed)
                {
                    ++m_uplinkSpeedTestsDone;
                    NX_DEBUG(this, "Received peerUplinkSpeed: %1", peerUplinkSpeed);
                },
				&m_uplinkSpeedUpdatedId);

		ASSERT_TRUE(m_uplinkSpeedUpdatedId != nx::utils::kInvalidSubscriptionId);

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

    void givenMediaserversOnMultipleSystems()
    {
		for (int i = 0; i < 5; ++i)
		{
			m_systems.emplace_back(addRandomSystem());
			for(int j = 0; j < 5; ++j)
				addServer(m_systems.back());
		}

        updateBestConnections();
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

	void saveConnectResponse(
		const std::string& systemId,
		api::Client::ResultCode result,
		api::ConnectResponse response)
	{
		QnMutexLocker lock(&m_mutex);
		m_connectResponses.emplace(
			systemId,
			std::make_pair(std::move(result), std::move(response)));
	}

	void whenClientsConnectToSystems()
	{
		for (const auto& system: m_systems)
		{
			auto& client = m_clients.emplace(system.id.constData(), httpUrl()).first->second;

			api::ConnectRequest request;
			request.connectionMethods = api::ConnectionMethod::all;
			request.destinationHostName = system.id;
			request.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();
			request.connectSessionId = QnUuid::createUuid().toSimpleByteArray();

			client.initiateConnection(
				request,
				[this, systemId = system.id.toStdString()](auto result, auto connectResponse)
				{
					saveConnectResponse(
						systemId,
						std::move(result),
						std::move(connectResponse));
				});
		}
	}

	void thenClientsConnectToServerWithBestUplinkSpeed()
	{
		const auto shouldWaitForConnectResponses =
			[this]()
			{
				QnMutexLocker lock(&m_mutex);
				return m_connectResponses.size() != m_systems.size();
			};

		while (shouldWaitForConnectResponses())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		for (const auto& response: m_connectResponses)
		{
			// Ensuring that connection succeeded.
			ASSERT_EQ(response.second.first, api::Client::ResultCode::ok);

			// Ensuring that the ConnectReponse received corresponds to the requested system.
			ASSERT_NE(
				-1,
				response.second.second.destinationHostFullName.indexOf(response.first.c_str()));

			// Ensuring that server connected to belongs to the requested system.
			ASSERT_EQ(
				response.second.second.destinationHostFullName,
				m_fastestServers[response.first]->mediaserver->fullName());
		}
	}

private:
    void addServer(const AbstractCloudDataProvider::System& system)
    {
        const auto keepAssigningBandwidth =
            [this](int bandwidth)
        {
            return std::find_if(m_servers.begin(), m_servers.end(),
                [&bandwidth](const auto& element)
                {
                    return bandwidth == element.second.uplinkSpeed.connectionSpeed.bandwidth;
                }) != m_servers.end();
        };

		TestContext* testContext = nullptr;
        {
            auto mediaServer = addRandomServer(
				system,
                boost::none,
                ServerTweak::defaultBehavior,
                network::http::kUrlSchemeName);
            ASSERT_NE(nullptr, mediaServer);

            QnMutexLocker lock(&m_mutex);

			testContext = &m_servers.emplace(system.id.constData(), TestContext())->second;
			testContext->mediaserver = std::move(mediaServer);

            testContext->uplinkSpeed.serverId =
                nx::toStdString(testContext->mediaserver->serverId());
			testContext->uplinkSpeed.systemId = system.id.constData();

            // Ensuring every bandwidth is unique.
            int bandwidth = nx::utils::random::number(10, 10000);
            while (keepAssigningBandwidth(bandwidth))
                bandwidth = nx::utils::random::number(10, 10000);

            testContext->uplinkSpeed.connectionSpeed.bandwidth = bandwidth;
            testContext->uplinkSpeed.connectionSpeed.pingTime =
                std::chrono::microseconds(nx::utils::random::number(10, 10000));

            testContext->speedTestUrl = lm("http://speedtest.%1.com").arg(m_servers.size() - 1);
        }

        ASSERT_EQ(api::ResultCode::ok, testContext->mediaserver->listen().first);

        testContext->uplinkSpeedReporter = std::make_unique<UplinkSpeedReporter>(
            &testContext->mediaserver->mediatorConnector(),
            testContext->speedTestUrl);

        NX_DEBUG(this, "Added a new server, index: %1, uplinkSpeed: %3",
            m_servers.size() - 1, testContext->uplinkSpeed);
    }

    void updateBestConnections()
    {
		for (const auto& server: m_servers)
		{
			auto [it, firstInsertion] = m_fastestServers.emplace(
				server.second.uplinkSpeed.systemId,
				&server.second);
			if (!firstInsertion)
			{
				if (server.second.uplinkSpeed.connectionSpeed.bandwidth >
					it->second->uplinkSpeed.connectionSpeed.bandwidth)
				{
					it->second = &server.second;
				}
			}
		}

		for (const auto& server : m_fastestServers)
		{
			NX_DEBUG(this, "Decided best uplink speed for system %1: %2",
				server.first, server.second);
		}
    }

private:
    UplinkSpeedTesterFactory::Function m_factoryFuncBak;
    QnMutex m_mutex;
	std::vector<AbstractCloudDataProvider::System> m_systems;
    std::multimap<std::string /*systemId*/, TestContext> m_servers;
	std::map<std::string/*systemId*/, const TestContext*> m_fastestServers;
	std::map<std::string/*systemId*/, api::Client> m_clients;
    std::atomic_int m_uplinkSpeedTestsDone = 0;
    nx::utils::SubscriptionId m_uplinkSpeedUpdatedId = nx::utils::kInvalidSubscriptionId;
	std::map<
		std::string /*systemId*/,
		std::pair<api::Client::ResultCode, api::ConnectResponse>> m_connectResponses;
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

TEST_F(SpeedTestEndToEnd, clients_connect_to_mediaserver_with_best_uplink_speed_in_each_system)
{
	givenMediator();
	givenMediaserversOnMultipleSystems();

	whenSpeedTestResultsAreReportedToMediator();
	whenClientsConnectToSystems();

	thenClientsConnectToServerWithBestUplinkSpeed();
}

} // namespace nx::hpm::test
