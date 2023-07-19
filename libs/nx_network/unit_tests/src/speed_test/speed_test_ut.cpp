// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/app_info.h>
#include <nx/network/aio/scheduler.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester_factory.h>
#include <nx/network/cloud/speed_test/uplink_speed_reporter.h>
#include <nx/network/cloud/speed_test/http_api_paths.h>
#include <nx/network/cloud/test_support/cloud_modules_xml_server.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/test_support/uplink_speed_test_server.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::cloud::speed_test::test {

using namespace cloud::test;

namespace {

static constexpr char kDiscoveryPrefix[] = "/discovery";

class MockUplinkSpeedTester: public speed_test::UplinkSpeedTester
{
public:
    MockUplinkSpeedTester(
        const Settings& settings,
        nx::utils::SyncQueue<std::tuple<
            SystemError::ErrorCode,
            std::optional<nx::hpm::api::ConnectionSpeed>>>* testRunEvent):
        UplinkSpeedTester(settings),
        m_testRunEvent(testRunEvent)
    {
    }

    virtual void start(CompletionHandler handler) override
    {
        UplinkSpeedTester::start(
            [this, handler = std::move(handler)](const auto systemError, const auto uplinkSpeed)
            {
                m_testRunEvent->push(std::make_tuple(systemError, uplinkSpeed));
                handler(systemError, std::move(uplinkSpeed));
            });
    }

private:
    nx::utils::SyncQueue<std::tuple<
        SystemError::ErrorCode,
        std::optional<nx::hpm::api::ConnectionSpeed>>>* m_testRunEvent;
};

} // namespace

class TestFixture: public testing::Test
{
protected:
	virtual void SetUp() override
	{
        m_testHttpServer = std::make_unique<nx::network::http::TestHttpServer>();
		ASSERT_TRUE(m_testHttpServer->bindAndListen());
        m_speedTestServer.registerRequestHandlers(
            std::string(),
            &m_testHttpServer->httpMessageDispatcher());
	}

    virtual void TearDown() override
    {
        // Need to stop the server before destructors are called to stop it serving requests to
        // destroyed objects in the child classes.
        m_testHttpServer.reset();
    }

    nx::utils::Url testHttpServerUrl() const
    {
        return url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress());
    }

	nx::utils::Url validSpeedTestUrl() const
	{
        return testHttpServerUrl();
	}

	nx::utils::Url invalidSpeedTestUrl() const
	{
		return "http://127.0.0.1:1";
	}

	void thenTestSucceeds()
	{
		auto errorCode = SystemError::noError;
		std::tie(errorCode, m_result) = m_testDoneEvent.pop();
		ASSERT_EQ(SystemError::noError, errorCode);
	}

	void thenTestFails()
	{
		auto errorCode = SystemError::noError;
		std::tie(errorCode, m_result) = m_testDoneEvent.pop();
		ASSERT_NE(SystemError::noError, errorCode);
	}

	void andTestResultIsValid()
	{
		ASSERT_TRUE(m_result);
	}

	void andTestResultIsInvalid()
	{
		ASSERT_FALSE(m_result);
	}

protected:
    std::unique_ptr<nx::network::http::TestHttpServer> m_testHttpServer;
	UplinkSpeedTestServer m_speedTestServer;
    AbstractSpeedTester::Settings m_speedTestSettings{{}, 20, 5, std::chrono::seconds(30)};

	nx::utils::SyncQueue<std::tuple<
		SystemError::ErrorCode,
		std::optional<nx::hpm::api::ConnectionSpeed>>> m_testDoneEvent;

	std::optional<nx::hpm::api::ConnectionSpeed> m_result;
};

class UplinkSpeedTester: public TestFixture
{
protected:
    void whenStartSpeedTest(const nx::utils::Url& url)
    {
        m_speedTestSettings.url = url;
        m_speedTester = std::make_unique<speed_test::UplinkSpeedTester>(m_speedTestSettings);
        m_speedTester->start(
			[this](auto&& ... args)
			{
				m_testDoneEvent.push(std::make_tuple(std::forward<decltype(args)>(args)...));
			});
    }

private:
    std::unique_ptr<speed_test::UplinkSpeedTester> m_speedTester;
};

TEST_F(UplinkSpeedTester, succeeds_with_valid_url)
{
	whenStartSpeedTest(validSpeedTestUrl());
	thenTestSucceeds();
	andTestResultIsValid();
}

TEST_F(UplinkSpeedTester, fails_with_invalid_url)
{
    whenStartSpeedTest(invalidSpeedTestUrl());
    thenTestFails();
    andTestResultIsInvalid();
}

//-------------------------------------------------------------------------------------------------

class UplinkSpeedReporter: public TestFixture
{
public:
    ~UplinkSpeedReporter()
    {
        m_reporter->pleaseStopSync();
        if (m_factoryFuncBak)
            UplinkSpeedTesterFactory::instance().setCustomFunc(std::move(m_factoryFuncBak));
    }

protected:
    using FetchMediatorAddressResult =
        std::pair<network::http::StatusCode::Value, nx::hpm::api::MediatorAddress>;

    virtual void SetUp() override
    {
        TestFixture::SetUp();

        overrideSpeedTestFactory();
        initializeFakeMediator();

        m_cloudModulesServer.registerRequestHandlers(
            kDiscoveryPrefix, &m_testHttpServer->httpMessageDispatcher());

        m_mediatorConnector = std::make_unique<hpm::api::MediatorConnector>(
            testHttpServerUrl().host().toStdString());

        m_mediatorConnector->mockupCloudModulesXmlUrl(cloudModulesXmlUrl());

        initializeUplinkSpeedReporter(m_speedTestSettings);
    }

    void givenValidCloudModules()
    {
        m_cloudModulesServer.setSpeedTestUrl(validSpeedTestUrl());
        m_cloudModulesServer.setHpmTcpUrl(mediatorHttpUrl());
        // Stun url is required for hpm::api::MediatorConnector to fetch correctly
        m_cloudModulesServer.setHpmUdpUrl("stun://dummy");
    }

    void givenCloudModulesWithInvalidMediatorAddress()
    {
        m_cloudModulesServer.setSpeedTestUrl(validSpeedTestUrl());
        // Skipping mediator urls
    }

    void givenSpeedTestWasRun()
    {
        whenSetSystemCredentials();
        thenSpeedTestIsRun();
    }

    void givenRunningBandwidthTest()
    {
        m_speedTestServer.setBandwidthTestInProgressHandler(
            [this](int xTestSequence)
            {
                m_bandwidthTestSequence = xTestSequence;
            });

        whenSetSystemCredentials(); //< Start the test.

        while (m_bandwidthTestSequence < 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void whenSetSystemCredentials()
    {
        // Setting SystemCredentials with a valid structure causes UplinkSpeedReporter
        // to receive systemCredentialsSet event, starting the test.

        hpm::api::SystemCredentials credentials {
            "systemId1",
            "serverId1",
            "someKey",
        };
        m_mediatorConnector->setSystemCredentials(std::move(credentials));

        m_reporter->start();
    }

    FetchMediatorAddressResult whenFetchMediatorAddress()
    {
        return m_fetchMediatorAddressEvent.pop();
    }

    void whenConnectionToSpeedTestServerIsBroken()
    {
        m_testHttpServer.reset();
    }

    void thenFetchMediatorAddressFailed(const FetchMediatorAddressResult& mediatorAddressResult)
    {
        ASSERT_NE(network::http::StatusCode::ok, mediatorAddressResult.first);
    }

    void thenFetchMediatorAddressSucceeded(const FetchMediatorAddressResult& mediatorAddressResult)
    {
        ASSERT_EQ(network::http::StatusCode::ok, mediatorAddressResult.first);
        ASSERT_EQ(mediatorHttpUrl(), mediatorAddressResult.second.tcpUrl);
    }

    void thenSpeedTestIsRun()
    {
        thenTestSucceeds();
        andTestResultIsValid();
    }

    void thenMediatorAddressIsFetched()
    {
        const auto mediatorAddress = whenFetchMediatorAddress();
        thenFetchMediatorAddressSucceeded(mediatorAddress);
    }

    void thenUplinkSpeedIsReportedToMediator()
    {
        NX_DEBUG(this, __func__);
        ASSERT_TRUE(m_reportReceivedEvent.pop());
    }

    std::size_t scheduleSize() const
    {
        return m_scheduler->schedule().size();
    }

    void initializeUplinkSpeedReporter(const AbstractSpeedTester::Settings& settings)
    {
        m_speedTestSettings = settings;

        auto scheduler = std::make_unique<network::aio::Scheduler>(
            std::chrono::milliseconds(10),
            std::set{std::chrono::milliseconds(9)});
        m_scheduler = scheduler.get();

        m_reporter = std::make_unique<speed_test::UplinkSpeedReporter>(
            cloudModulesXmlUrl(),
            m_mediatorConnector.get(),
            std::move(scheduler),
            settings);

        m_reporter->setFetchMediatorAddressHandler(
            [this](const auto& httpStatusCode, const auto& mediatorAddress)
            {
                m_fetchMediatorAddressEvent.push({httpStatusCode, mediatorAddress});
            });
    }

private:
    void overrideSpeedTestFactory()
    {
        m_factoryFuncBak = UplinkSpeedTesterFactory::instance().setCustomFunc(
            [this](const auto& speedTestSettings)
            {
                // Forcing speed test to complete immediately when start() is called
                return std::make_unique<MockUplinkSpeedTester>(
                    speedTestSettings,
                    &m_testDoneEvent);
            });
    }

    void initializeFakeMediator()
    {
        // TODO remove this handler when vms 4.1 beta is done and
        // hpm::api::kConnectionSpeedUplinkPath removed.
        ASSERT_TRUE(m_fakeMediator.bindAndListen());
        m_fakeMediator.httpMessageDispatcher().registerRequestProcessorFunc(
            network::http::Method::post,
            hpm::api::kConnectionSpeedUplinkPath,
            [this](
                nx::network::http::RequestContext /*request*/,
                nx::network::http::RequestProcessedHandler completionHandler)
            {
                m_reportReceivedEvent.push(true);
                completionHandler(nx::network::http::StatusCode::ok);
            });

        m_fakeMediator.httpMessageDispatcher().registerRequestProcessorFunc(
            network::http::Method::post,
            hpm::api::kConnectionSpeedUplinkPathV2,
            [this](
                nx::network::http::RequestContext /*request*/,
                nx::network::http::RequestProcessedHandler completionHandler)
            {
                m_reportReceivedEvent.push(true);
                completionHandler(nx::network::http::StatusCode::ok);
            });
    }

    nx::utils::Url cloudModulesXmlUrl() const
    {
        return url::Builder(testHttpServerUrl()).setPath(
            m_cloudModulesServer.cloudModulesXmlPath());
    }

    nx::utils::Url mediatorHttpUrl() const
    {
        return network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(m_fakeMediator.serverAddress());
    }

private:
    CloudModulesXmlServer m_cloudModulesServer;
    nx::network::http::TestHttpServer m_fakeMediator;
    nx::utils::SyncQueue<bool> m_reportReceivedEvent;
    nx::utils::SyncQueue<FetchMediatorAddressResult> m_fetchMediatorAddressEvent;
    nx::utils::MoveOnlyFunc<speed_test::UplinkSpeedTesterFactoryType> m_factoryFuncBak;
    nx::network::aio::Scheduler* m_scheduler = nullptr;
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    std::unique_ptr<speed_test::UplinkSpeedReporter> m_reporter;
    std::atomic_int m_bandwidthTestSequence = 0;
};

TEST_F(UplinkSpeedReporter, fetches_speed_test_url_and_performs_test_and_reports_results)
{
    givenValidCloudModules();

    whenSetSystemCredentials();

    // Test is expected to run one extra time due to system crednetials being set
    for (std::size_t i = 0; i < scheduleSize() + 1; ++i)
    {
        thenSpeedTestIsRun();
        thenMediatorAddressIsFetched();
        thenUplinkSpeedIsReportedToMediator();
    }
}

TEST_F(UplinkSpeedReporter, doesnt_crash_when_fetching_mediator_address_fails)
{
    givenCloudModulesWithInvalidMediatorAddress();
    givenSpeedTestWasRun();

    const auto mediatorAddress = whenFetchMediatorAddress();

    thenFetchMediatorAddressFailed(mediatorAddress);

    // and process does not crash;
}

TEST_F(UplinkSpeedReporter, does_not_crash_during_bandwidth_test_if_connection_breaks)
{
    givenValidCloudModules();

    AbstractSpeedTester::Settings s;
    s.maxPingRequests = 1;
    s.minBandwidthRequests = std::numeric_limits<int>::max();
    s.testDuration = std::chrono::hours(1);
    initializeUplinkSpeedReporter(s);
    givenRunningBandwidthTest();

    whenConnectionToSpeedTestServerIsBroken();

    // then process does not crash;
}

} // namespace nx::network::cloud::speed_test::test
