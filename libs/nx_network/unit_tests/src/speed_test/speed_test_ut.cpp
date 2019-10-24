#include <gtest/gtest.h>

#include <nx/network/aio/scheduler.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester_factory.h>
#include <nx/network/cloud/speed_test/uplink_speed_reporter.h>
#include <nx/network/cloud/speed_test/http_api_paths.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::cloud::speed_test::test {

namespace {

class UplinkSpeedTestServer
{
public:
    UplinkSpeedTestServer()
    {
		m_server.registerRequestProcessorFunc(
			speed_test::http::kApiPath,
			[this](auto&& ... args) { serve(std::forward<decltype(args)>(args)...); });
    }

    bool bindAndListen()
    {
        return m_server.bindAndListen();
    }

    nx::utils::Url url() const
    {
        return url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_server.serverAddress());
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

private:
    nx::network::http::TestHttpServer m_server;
};

class MockUplinkSpeedTester: public AbstractSpeedTester
{
public:
    MockUplinkSpeedTester(
        nx::utils::SyncQueue<std::tuple<
            SystemError::ErrorCode,
            std::optional<nx::hpm::api::ConnectionSpeed>>>* testRunEvent):
        m_testRunEvent(testRunEvent)
    {
    }

    virtual void start(const nx::utils::Url& /*url*/, CompletionHandler handler) override
    {
        m_testRunEvent->push(std::make_tuple(SystemError::noError, hpm::api::ConnectionSpeed{}));
        handler(SystemError::noError, hpm::api::ConnectionSpeed{});
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
		ASSERT_TRUE(m_speedTestServer.bindAndListen());
	}

	nx::utils::Url validSpeedTestUrl()
	{
		return m_speedTestServer.url();
	}

	nx::utils::Url invalidSpeedTestUrl()
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
	UplinkSpeedTestServer m_speedTestServer;

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
        m_speedTester = std::make_unique<speed_test::UplinkSpeedTester>();
        m_speedTester->start(
			url,
			[this](auto&& ... args)
			{
				m_testDoneEvent.push(std::make_tuple(std::forward<decltype(args)>(args)...));
			});
    }

private:
    std::unique_ptr<speed_test::UplinkSpeedTester> m_speedTester;
};

TEST_F(UplinkSpeedTester, succeeds_with_valid_url_local)
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
    virtual void SetUp() override
    {
        initializeFakeMediator();
        mockupFactoryFunc();

        m_mediatorConnector = std::make_unique<hpm::api::MediatorConnector>("");

        hpm::api::MediatorAddress address;
        address.tcpUrl = url::Builder().setEndpoint(m_fakeMediator.serverAddress())
            .setScheme(network::http::kUrlSchemeName);
        m_mediatorConnector->mockupMediatorAddress(std::move(address));

        using namespace nx::network::aio;

        auto scheduler = std::make_unique<Scheduler>(
            std::chrono::milliseconds(100),
            std::set{std::chrono::milliseconds(99)});

        m_scheduler = scheduler.get();

        m_reporter = std::make_unique<speed_test::UplinkSpeedReporter>(
            m_mediatorConnector.get(),
            m_speedTestServer.url(),
            std::move(scheduler));

        m_reporter->setAboutToRunSpeedTestHandler(
            [this](bool aboutToRunSpeedTest)
            {
                NX_DEBUG(this, "About to start speed test: %1", aboutToRunSpeedTest);
                ++m_testAttempts;
            });
    }

    void whenSetSystemCredentials()
    {
        // Setting SystemCredentials with a valid structure causes UplinkSpeedReporter
        // to receive systemCredentialsSet event, starting the test.
        m_mediatorConnector->setSystemCredentials(hpm::api::SystemCredentials());
    }

    void thenSpeedTestIsRun()
    {
        thenTestSucceeds();
        andTestResultIsValid();
    }

    void thenUplinkSpeedIsReportedToMediator()
    {
        ASSERT_TRUE(m_reportReceivedEvent.pop());
    }

    void andTestIsPerformedExpectedNumberOfTimes()
    {
        // NOTE: <= because the SpeedTestReporter invokes one additional speed test when the
        // System credentials are set.
        while (m_testAttempts <= (int) m_scheduler->schedule().size())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    void mockupFactoryFunc()
    {
        m_factoryFuncBak = UplinkSpeedTesterFactory::instance().setCustomFunc(
            [this]()
            {
                // Forcing speed test to complete immediately when start() is called
                return std::make_unique<MockUplinkSpeedTester>(&m_testDoneEvent);
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

private:
    nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractSpeedTester>(void)> m_factoryFuncBak;
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    std::unique_ptr<speed_test::UplinkSpeedReporter> m_reporter;
    nx::network::http::TestHttpServer m_fakeMediator;
    nx::utils::SyncQueue<bool> m_reportReceivedEvent;
    std::atomic_int m_testAttempts = 0;
    nx::network::aio::Scheduler* m_scheduler = nullptr;
};

TEST_F(UplinkSpeedReporter, fetches_speed_test_url_and_performs_test_and_reports_results)
{
    whenSetSystemCredentials();
    thenSpeedTestIsRun();
    thenUplinkSpeedIsReportedToMediator();
    andTestIsPerformedExpectedNumberOfTimes();
}

} // namespace nx::network::cloud::speed_test::test