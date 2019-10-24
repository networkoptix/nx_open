#include "uplink_speed_tester.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/time.h>

#include "http_api_paths.h"

namespace nx::network::cloud::speed_test {

using namespace std::chrono;
using namespace nx::hpm::api;

namespace {

static constexpr int kMaxPingRequests = 10;
static const milliseconds kTestDuration = seconds(3);

} // namespace

UplinkSpeedTester::~UplinkSpeedTester()
{
	pleaseStopSync();
}

void UplinkSpeedTester::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
	if (m_httpClient)
		m_httpClient->bindToAioThread(aioThread);
	if (m_bandwidthTester)
		m_bandwidthTester->bindToAioThread(aioThread);
}

void UplinkSpeedTester::stopWhileInAioThread()
{
	m_httpClient.reset();
	m_bandwidthTester.reset();
}

void UplinkSpeedTester::start(const nx::utils::Url& speedTestUrl, CompletionHandler handler)
{
	dispatch(
		[this, speedTestUrl, handler = std::move(handler)]() mutable
		{
			m_url = speedTestUrl;
			m_handler = std::move(handler);
			m_httpClient = std::make_unique<network::http::AsyncClient>();
			startPingTest();
		});
}

void UplinkSpeedTester::startBandwidthTest(const microseconds& pingTime)
{
	m_bandwidthTester =
		std::make_unique<UplinkBandwidthTester>(m_url, kTestDuration, pingTime);
	m_bandwidthTester->doBandwidthTest(
		[this, pingTime](SystemError::ErrorCode errorCode, int bandwidth)
		{
			if (errorCode != SystemError::noError)
				return emitTestResult(errorCode, std::nullopt);

            emitTestResult(SystemError::noError, ConnectionSpeed{pingTime, bandwidth});
		});
}

void UplinkSpeedTester::setupPingTest()
{
	m_httpClient->setOnDone(
		[this]()
		{
			if (m_httpClient->failed())
			{
				m_httpClient->setOnResponseReceived(nullptr);
				auto errorCode = m_httpClient->lastSysErrorCode();
				return emitTestResult(errorCode, std::nullopt);
			}
		});

    m_httpClient->setOnResponseReceived(
        [this]
        {
            auto now = nx::utils::utcTime();
            auto pingTime = duration_cast<microseconds>(now - m_testContext.requestSentTime);

			// Ignoring first request because tcp uses two round trips to establish
			// a connection, increasing the ping time on first request.
            if (++m_testContext.totalPings == 1)
            {
                NX_VERBOSE(this, "Initial ping: %1", pingTime);
                return sendPing();
            }

            m_testContext.totalPingTime += pingTime;

            auto averagePingTime =
                m_testContext.totalPingTime / m_testContext.totalPings;
            NX_VERBOSE(this, "Average ping: %1", averagePingTime);

            if (now - m_testContext.startTime < kTestDuration
				&& m_testContext.totalPings < kMaxPingRequests)
            {
                sendPing();
            }
            else
            {
				startBandwidthTest(averagePingTime);

				m_httpClient.reset();
            }
        });
}

void UplinkSpeedTester::startPingTest()
{
    setupPingTest();
	m_testContext.startTime = nx::utils::utcTime();
    sendPing();
}

void UplinkSpeedTester::sendPing()
{
    m_httpClient->doGet(url::Builder(m_url).setPath(http::kApiPath));
    m_testContext.requestSentTime = nx::utils::utcTime();
}

void UplinkSpeedTester::emitTestResult(
    SystemError::ErrorCode errorCode,
    std::optional<ConnectionSpeed> result)
{
    QString resultStr = result
        ? lm("{pingTime: %1, bandwidth: %2 Bpms (%3 Mbps)}")
        .args(result->pingTime, result->bandwidth, result->bandwidth * kBytesPerMsecToMegabitsPerSec)
		: "none";

	NX_VERBOSE(this, "Test complete, reporting system error: '%1' and speed test result: %2",
		SystemError::toString(errorCode), resultStr);

    if (m_handler)
		nx::utils::swapAndCall(m_handler, errorCode, std::move(result));
}


} // namespace nx::network::cloud::speed_test