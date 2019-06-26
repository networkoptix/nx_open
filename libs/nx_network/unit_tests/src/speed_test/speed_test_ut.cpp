#include <gtest/gtest.h>

#include <nx/network/cloud/speed_test/uplink_speed_tester.h>
#include <nx/network/cloud/speed_test/http_api_paths.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
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

class ScheduledUplinkSpeedTester: public TestFixture
{
protected:
    void whenScheduleSpeedTest()
    {
		using namespace std::chrono;

		m_speedTester = std::make_unique<speed_test::ScheduledUplinkSpeedTester>();

		m_speedTester->start(
			validSpeedTestUrl(),
			[this](auto&&... args)
			{
				++m_completeTests;
				m_testDoneEvent.push(std::make_tuple(std::forward<decltype(args)>(args)...));
			});
    }

	void thenTestIsScheduledWithinRange()
		{
		for (const auto& testTime : m_speedTester->testSchedule())
        {
            ASSERT_TRUE(
                m_speedTester->kMinTime <= testTime && testTime <= m_speedTester->kMaxTime);
	}

        auto timeout = m_speedTester->waitTimeBeforeNextTest();
        ASSERT_TRUE(timeout != m_speedTester->kInvalid);
        ASSERT_TRUE(m_speedTester->waitTimeBeforeNextTest() < std::chrono::hours(24));
	}

private:
    std::unique_ptr<speed_test::ScheduledUplinkSpeedTester> m_speedTester;
	std::atomic_int m_completeTests = 0;
};

TEST_F(ScheduledUplinkSpeedTester, performs_tests_on_schedule)
{
	whenScheduleSpeedTest();
    thenTestIsScheduledWithinRange();
}

} // namespace nx::network::cloud::speed_test::test