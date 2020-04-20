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
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::cloud::speed_test::test {

using namespace cloud::test;

namespace {

static constexpr char kDiscoveryPrefix[] = "/discovery";

class UplinkSpeedTestServer
{
public:
    void registerRequestHandlers(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher)
    {
		messageDispatcher->registerRequestProcessorFunc(
            network::http::kAnyMethod,
			url::joinPath(basePath, speed_test::http::kApiPath),
			[this](auto&& ... args) { serve(std::forward<decltype(args)>(args)...); });
    }

private:
    void serve(
        nx::network::http::RequestContext request,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
		auto it = request.request.headers.find("X-Test-Sequence");
		if (it != request.request.headers.end())
		{
			nx::network::http::insertOrReplaceHeader(&request.response->headers,
				network::http::HttpHeader(it->first, it->second));
		}
        completionHandler(nx::network::http::StatusCode::ok);
    }
};

class MockUplinkSpeedTester: public UplinkSpeedTester
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

class TestFixture : public testing::Test
{
protected:
	virtual void SetUp() override
	{
		ASSERT_TRUE(m_testHttpServer.bindAndListen());
        m_speedTestServer.registerRequestHandlers(
            std::string(),
            &m_testHttpServer.httpMessageDispatcher());
	}

    nx::utils::Url testHttpServerUrl() const
    {
        return url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(m_testHttpServer.serverAddress());
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
    nx::network::http::TestHttpServer m_testHttpServer;
	UplinkSpeedTestServer m_speedTestServer;
    AbstractSpeedTester::Settings m_speedTestSettings{{}, 20, std::chrono::seconds(30)};

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
        if (m_factoryFuncBak)
            UplinkSpeedTesterFactory::instance().setCustomFunc(std::move(m_factoryFuncBak));

        m_reporter->pleaseStopSync();
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
            kDiscoveryPrefix, &m_testHttpServer.httpMessageDispatcher());

        m_mediatorConnector = std::make_unique<hpm::api::MediatorConnector>(
            testHttpServerUrl().host().toStdString());

        m_mediatorConnector->mockupCloudModulesXmlUrl(cloudModulesXmlUrl());

        auto scheduler = std::make_unique<network::aio::Scheduler>(
            std::chrono::milliseconds(100),
            std::set{std::chrono::milliseconds(99)});

        m_scheduler = scheduler.get();

        m_reporter = std::make_unique<speed_test::UplinkSpeedReporter>(
            cloudModulesXmlUrl(),
            m_mediatorConnector.get(),
            std::move(scheduler));

        m_reporter->setAboutToRunSpeedTestHandler(
            [this](bool aboutToRunSpeedTest)
            {
                NX_DEBUG(this, "About to start speed test: %1", aboutToRunSpeedTest);
                if (aboutToRunSpeedTest)
                    ++m_testAttempts;
            });

        m_reporter->setFetchMediatorAddressHandler(
            [this](const auto& httpStatusCode, const auto& mediatorAddress)
            {
                m_fetchMediatorAddressEvent.push({httpStatusCode, mediatorAddress});
            });
    }

    void givenValidCloudModules()
    {
        m_cloudModulesServer.setSpeedTestUrl(validSpeedTestUrl());
        m_cloudModulesServer.setHpmTcpUrl(mediatorHttpUrl());
        //stun url is required for hpm::api::MediatorConnector to fetch correctly
        m_cloudModulesServer.setHpmUdpUrl("stun://dummy");
    }

    void givenCloudModulesWithInvalidMediatorAddress()
    {
        m_cloudModulesServer.setSpeedTestUrl(validSpeedTestUrl());
        //Skipping mediator urls
    }

    void givenSpeedTestWasRun()
    {
        whenSetSystemCredentials();
        thenSpeedTestIsRun();
    }

    void whenSetSystemCredentials()
    {
        // Setting SystemCredentials with a valid structure causes UplinkSpeedReporter
        // to receive systemCredentialsSet event, starting the test.
        m_mediatorConnector->setSystemCredentials(hpm::api::SystemCredentials());
        m_reporter->start();
    }

    FetchMediatorAddressResult whenFetchMediatorAddress()
    {
        return m_fetchMediatorAddressEvent.pop();
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

    void andTestIsPerformedExpectedNumberOfTimes()
    {
        NX_DEBUG(this, __func__);
        // NOTE: <= because the SpeedTestReporter invokes one additional speed test when the
        // System credentials are set.
        while (m_testAttempts <= (int) m_scheduler->schedule().size())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    }

    nx::utils::Url cloudModulesXmlUrl() const
    {
        return url::Builder(testHttpServerUrl()).setPath(
            *m_cloudModulesServer.cloudModulesXmlPath());
    }

    nx::utils::Url mediatorHttpUrl() const
    {
        return network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(m_fakeMediator.serverAddress());
    }

private:
    CloudModulesXmlServer m_cloudModulesServer;
    nx::utils::MoveOnlyFunc<speed_test::UplinkSpeedTesterFactoryType> m_factoryFuncBak;
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    std::unique_ptr<speed_test::UplinkSpeedReporter> m_reporter;
    nx::network::http::TestHttpServer m_fakeMediator;
    nx::utils::SyncQueue<bool> m_reportReceivedEvent;
    std::atomic_int m_testAttempts = 0;
    nx::network::aio::Scheduler* m_scheduler = nullptr;
    nx::utils::SyncQueue<FetchMediatorAddressResult> m_fetchMediatorAddressEvent;
};

TEST_F(UplinkSpeedReporter, fetches_speed_test_url_and_performs_test_and_reports_results)
{
    givenValidCloudModules();

    whenSetSystemCredentials();

    thenSpeedTestIsRun();
    thenMediatorAddressIsFetched();
    thenUplinkSpeedIsReportedToMediator();

    andTestIsPerformedExpectedNumberOfTimes();
}

TEST_F(UplinkSpeedReporter, doesnt_crash_when_fetching_mediator_address_fails)
{
    givenCloudModulesWithInvalidMediatorAddress();
    givenSpeedTestWasRun();

    const auto mediatorAddress = whenFetchMediatorAddress();

    thenFetchMediatorAddressFailed(mediatorAddress);

    // and process does not crash;
}

} // namespace nx::network::cloud::speed_test::test
