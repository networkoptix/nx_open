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
static constexpr int kPayloadSizeBytes = 1000 * 1000;
static constexpr float kSimilarityThreshold = 0.97F;
static constexpr int kMinRunningAverages = 4;
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
    std::vector<float> bandwidths;

    std::transform(
        std::prev(end, kMinRunningAverages),
        end,
        std::inserter(bandwidths, bandwidths.begin()),
        [](const std::pair<const int, RunningValue>& elem)
        {
            return elem.second.averageBandwidth;
        });

    for (auto it = std::next(bandwidths.begin(), 1); it != bandwidths.end(); ++it)
    {
        if (!*bandwidths.begin() || !*it)
            return std::nullopt;

        if (*bandwidths.begin() / *it < kSimilarityThreshold)
            return std::nullopt;
    }

    return (int) bandwidths.front();
}

void UplinkBandwidthTester::onMessageReceived(network::http::Message message)
{
	auto sequence = parseSequence(message);
	if (!sequence)
		return testFailed(SystemError::invalidData);

    auto messageSentTime = nx::utils::utcTime() - m_pingTime;

	auto currentDuration = messageSentTime - m_testContext.startTime;
    if (currentDuration >= kMinTestDuration)
    {
        float runningTotalBytesSent = m_testContext.runningValues[*sequence].totalBytesSent;
        m_testContext.runningValues[*sequence].averageBandwidth =
            runningTotalBytesSent / duration_cast<milliseconds>(currentDuration).count();

        auto bytesPerMsec = stopEarlyIfAble(*sequence);
        if (bytesPerMsec)
        {
            const auto timeLeftUntilMessagesAreNotSent =
                m_testDuration - (nx::utils::utcTime() - m_testContext.startTime);
            NX_VERBOSE(this,
                "Stopping early on sequence: %1 with %2 Bpms (%3 Mbps), and %4 requests sent. "
                "Time left until no more messages are sent: %5",
                *sequence, *bytesPerMsec, *bytesPerMsec * kBytesPerMsecToMegabitsPerSec,
                m_testContext.sequence,
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
	m_testContext.runningValues[sequence].totalBytesSent = m_testContext.totalBytesSent;

    m_pipeline->sendData(
		std::move(buffer),
		[this](SystemError::ErrorCode errorCode)
		{
			if (errorCode != SystemError::noError)
				return testFailed(errorCode);

			sendRequest();
		});
}

void nx::network::cloud::speed_test::UplinkBandwidthTester::testComplete(int bandwidth)
{
    if (m_handler)
    {
        m_testContext = TestContext();
        nx::utils::swapAndCall(m_handler, SystemError::noError, bandwidth);
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
