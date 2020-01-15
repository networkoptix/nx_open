#include "uplink_bandwidth_tester.h"

#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/time.h>

#include "http_api_paths.h"

namespace nx::network::cloud::speed_test {

using namespace nx::network::http;
using namespace std::chrono;

namespace {

static constexpr char kSpeedTest[] = "SPEEDTEST";
static constexpr int kPayloadSizeBytes = 1000 * 1000 ; //< 1 Megabyte
static constexpr float kSimilarityThreshold = 0.97F;
static constexpr int kMinRunningAverages = 5;
static constexpr auto kMinTestDuration = milliseconds(1);

static QByteArray makePayload()
{
	QByteArray payload;
	payload.reserve(kPayloadSizeBytes);

	while (payload.size() + sizeof(kSpeedTest) <= kPayloadSizeBytes)
		payload += kSpeedTest;

	return payload;
}

} // namespace

UplinkBandwidthTester::UplinkBandwidthTester(
	const nx::utils::Url& url,
	const std::chrono::milliseconds& testDuration,
	const std::chrono::microseconds& pingTime)
	:
	m_url(url),
	m_testDuration(testDuration),
	m_pingTime(pingTime)
{
	m_url.setPath({});
}

UplinkBandwidthTester::~UplinkBandwidthTester()
{
	pleaseStopSync();
}

void UplinkBandwidthTester::bindToAioThread(aio::AbstractAioThread* aioThread)
{
	base_type::bindToAioThread(aioThread);
	if (m_pipeline)
		m_pipeline->bindToAioThread(aioThread);
	if (m_tcpSocket)
		m_tcpSocket->bindToAioThread(aioThread);
}

void UplinkBandwidthTester::stopWhileInAioThread()
{
	m_pipeline.reset();
	m_tcpSocket.reset();
}

void UplinkBandwidthTester::doBandwidthTest(BandwidthCompletionHandler handler)
{
	dispatch(
		[this, handler = std::move(handler)]() mutable
		{
			m_handler = std::move(handler);
			m_tcpSocket =
                std::make_unique<network::TCPSocket>(SocketFactory::tcpClientIpVersion());
			m_tcpSocket->bindToAioThread(getAioThread());
			m_tcpSocket->setNonBlockingMode(true);
			m_tcpSocket->connectAsync(
				network::url::getEndpoint(m_url),
				[this](SystemError::ErrorCode errorCode) {
					using namespace std::placeholders;

					NX_VERBOSE(this, "TCP connection to %1, complete, system error = %2",
						m_url, SystemError::toString(errorCode));

                    if (errorCode != SystemError::noError)
						return testFailed(errorCode);

					m_pipeline = std::make_unique<network::http::AsyncMessagePipeline>(
						std::exchange(m_tcpSocket, nullptr));

					m_pipeline->setMessageHandler(
						std::bind(&UplinkBandwidthTester::onMessageReceived, this, _1));

					m_pipeline->registerCloseHandler(
						std::bind(&UplinkBandwidthTester::testFailed, this, SystemError::connectionReset));

					m_pipeline->startReadingConnection();

					m_testContext = TestContext();
					m_testContext.payload = makePayload();
                    m_testContext.sendRequests = true;
                    m_testContext.startTime = nx::utils::utcTime();

					sendRequest();
            });
		});
}

std::pair<int, nx::Buffer> UplinkBandwidthTester::makeRequest()
{
	++m_testContext.sequence;

	Request request;
    request.headers.emplace("Date", formatDateTime(QDateTime::currentDateTime()));
    request.headers.emplace("User-Agent", userAgentString());
    request.headers.emplace("Host", url::getEndpoint(m_url).toString().toUtf8());
    request.headers.emplace("Content-Type", "text/plain");
	request.headers.emplace("Connection", "keep-alive");
	request.headers.emplace("Content-Length", std::to_string(m_testContext.payload.size()).c_str());
	request.headers.emplace("X-Test-Sequence", std::to_string(m_testContext.sequence).c_str());

	request.requestLine.method = Method::post;
	request.requestLine.url.setPath(http::kApiPath);
	request.requestLine.version = http_1_1;

	// Adding payload to the request before serializing results in a double copy of the payload:
	// Once to the request, and again into the buffer.
	return std::make_pair(
		m_testContext.sequence,
		request.serialized().append(m_testContext.payload));
}

std::optional<int> UplinkBandwidthTester::parseSequence(const network::http::Message& message)
{
	if (!message.response)
	{
		NX_VERBOSE(this, "Received message doesn't contain a Response");
		return std::nullopt;
	}

	auto it = message.headers().find("X-Test-Sequence");
	if (it == message.headers().end())
	{
		NX_VERBOSE(this, "Received message doesn't contain 'X-Test-Sequence' header");
		return std::nullopt;
	}

	return it->second.toInt();
}

std::optional<int> UplinkBandwidthTester::stopEarlyIfAble(int sequence) const
{
    if (sequence < kMinRunningAverages
        || (int) m_testContext.runningValues.size() < kMinRunningAverages)
    {
        return std::nullopt;
    }

    auto end = m_testContext.runningValues.find(sequence);
	NX_ASSERT(end != m_testContext.runningValues.end());
	auto begin = std::prev(end, kMinRunningAverages);

	NX_VERBOSE(this, "Comparing %1 bandwidths: %2, similarity threshold = %3",
		std::distance(begin, end), containerString(begin, end), kSimilarityThreshold);

    for (auto it = std::next(begin, 1); it != end; ++it)
    {
		const auto high = std::max(begin->second.averageBandwidth, it->second.averageBandwidth);
		const auto low = std::min(begin->second.averageBandwidth, it->second.averageBandwidth);

        if (!low || !high || low / high < kSimilarityThreshold)
            return std::nullopt;
    }

	return begin->second.averageBandwidth;
}

void UplinkBandwidthTester::onMessageReceived(network::http::Message message)
{
	if (!m_handler) //< If handler is nullptr, test is already completed and handler invoked
		return;

	auto sequence = parseSequence(message);
	if (!sequence)
		return testFailed(SystemError::invalidData);

	auto messageSentTime = nx::utils::utcTime() - m_pingTime;
	auto currentDuration = messageSentTime - m_testContext.startTime;

    if (currentDuration >= kMinTestDuration)
    {
		const auto it = m_testContext.runningValues.find(*sequence);
		NX_ASSERT(it != m_testContext.runningValues.end());
        it->second.averageBandwidth =
            (float) it->second.totalBytesSent / duration_cast<milliseconds>(currentDuration).count();

		NX_VERBOSE(this,
			"Calculated running value for sequence %1, totalBytesSent: %2, running value: %3", 
			*sequence, m_testContext.totalBytesSent, it->second);

        auto bytesPerMsec = stopEarlyIfAble(*sequence);
        if (bytesPerMsec)
        {
            const auto timeLeftUntilMessagesAreNotSent =
                m_testDuration - (nx::utils::utcTime() - m_testContext.startTime);
            NX_VERBOSE(this,
                "Stopping early on sequence: %1 with %2 bytes per msec, and %4 requests sent. "
                "Time left until no more messages are sent: %5",
                *sequence, *bytesPerMsec, m_testContext.sequence,
                duration_cast<milliseconds>(timeLeftUntilMessagesAreNotSent));
			
            m_testContext.sendRequests = false;
            return testComplete(*bytesPerMsec);
        }
    }

	if (!m_testContext.sendRequests && *sequence == m_testContext.sequence)
	{
		if (*sequence == 0 || currentDuration < kMinTestDuration)
			return testFailed(SystemError::invalidData);

        testComplete(
            m_testContext.totalBytesSent / duration_cast<milliseconds>(currentDuration).count());
	}
}

void UplinkBandwidthTester::sendRequest()
{
	if (!m_testContext.sendRequests)
		return;

	if (nx::utils::utcTime() - m_testContext.startTime >= m_testDuration)
	{
		m_testContext.sendRequests = false;
		return;
	}

	auto [sequence, buffer] = makeRequest();
	m_testContext.totalBytesSent += buffer.size();
	auto& runningValue = m_testContext.runningValues[sequence];
	runningValue.totalBytesSent = m_testContext.totalBytesSent;

	NX_VERBOSE(this, "Sending request %1, buffer size: %2", sequence, buffer.size());

    m_pipeline->sendData(
		std::move(buffer),
		[this](SystemError::ErrorCode errorCode)
		{
			if (errorCode != SystemError::noError)
				return testFailed(errorCode);

			sendRequest();
		});

	NX_VERBOSE(this, "Sent request %1, totalBytesSent: %2 running value: %3", 
		sequence, m_testContext.totalBytesSent,
		m_testContext.runningValues[sequence]);
}

void UplinkBandwidthTester::testComplete(int bytesPerMsec)
{
    if (m_handler)
    {
        m_testContext = TestContext();
		// * 8 converts to bits per msec, / 1024 to kilobits per msec, * 1000 to kilobits per sec
		const auto kilobitsPerSec = ((long long)bytesPerMsec * 8 / 1024) * 1000;

		NX_VERBOSE(this, "Test complete, reporting bytes per msec %1 (%2 kilobits per sec)",
			bytesPerMsec, (int) kilobitsPerSec);

        nx::utils::swapAndCall(m_handler, SystemError::noError, (int) kilobitsPerSec);
    }
}

void UplinkBandwidthTester::testFailed(SystemError::ErrorCode errorCode)
{
    if (m_handler)
    {
        m_testContext = TestContext();
        nx::utils::swapAndCall(m_handler, errorCode, 0);
    }
}

} // namespace nx::network::cloud::speed_test
