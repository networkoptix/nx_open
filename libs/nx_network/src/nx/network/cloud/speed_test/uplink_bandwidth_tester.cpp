// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uplink_bandwidth_tester.h"

#include <nx/utils/datetime.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>

#include "http_api_paths.h"

namespace nx::network::cloud::speed_test {

using namespace nx::network::http;
using namespace std::chrono;

namespace {

static constexpr char kSpeedTest[] = "SPEEDTEST";
static constexpr int kPayloadSizeBytes = 1000 * 1000; //< 1 Megabyte
static constexpr float kSimilarityThreshold = 0.97F;
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

std::string UplinkBandwidthTester::RunningValue::toString() const
{
    return nx::format("{ totalBytesSent = %1, averageBandwidth = %2 bytes per msec }")
        .args(totalBytesSent, averageBandwidth).toStdString();
}

UplinkBandwidthTester::UplinkBandwidthTester(
    const nx::utils::Url& url,
    const std::chrono::milliseconds& testDuration,
    int minBandwidthRequests,
    const std::chrono::microseconds& pingTime)
    :
    m_url(url),
    m_testDuration(testDuration),
    m_minBandwidthRequests(minBandwidthRequests),
    m_pingTime(pingTime)
{
    NX_VERBOSE(this, "url: %1, testDuration: %2, minBandwidthRequests: %3, pingTime: %4",
        url, testDuration, minBandwidthRequests, pingTime);
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
                [this](SystemError::ErrorCode errorCode)
                {
                    using namespace std::placeholders;

                    NX_VERBOSE(this, "TCP connection to %1, complete, system error = %2",
                        m_url, SystemError::toString(errorCode));

                    if (errorCode != SystemError::noError)
                        return testFailed(errorCode, "failed to connect to endpoint");

                    m_pipeline = std::make_unique<network::http::AsyncMessagePipeline>(
                        std::exchange(m_tcpSocket, nullptr));

                    m_pipeline->setMessageHandler(
                        std::bind(&UplinkBandwidthTester::onMessageReceived, this, _1));

                    const auto guard = m_asyncGuard.sharedGuard();
                    m_pipeline->registerCloseHandler(
                        [this, guard](auto error, auto /* connectionDestroyed */)
                        {
                            if (auto lock = guard->lock())
                                testFailed(error, "registerCloseHandler");
                        });

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
    request.headers.emplace("Date", nx::utils::formatDateTime(QDateTime::currentDateTime()));
    request.headers.emplace("User-Agent", userAgentString());
    request.headers.emplace("Host", url::getEndpoint(m_url).toString());
    request.headers.emplace("Content-Type", "text/plain");
    request.headers.emplace("Connection", "keep-alive");
    request.headers.emplace("Content-Length", std::to_string(m_testContext.payload.size()));
    request.headers.emplace("X-Test-Sequence", std::to_string(m_testContext.sequence));

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

    return nx::utils::stoi(it->second);
}

std::optional<int> UplinkBandwidthTester::stopEarlyIfAble(int sequence) const
{
    if (sequence < m_minBandwidthRequests
        || (int) m_testContext.runningValues.size() < m_minBandwidthRequests)
    {
        return std::nullopt;
    }

    auto end = m_testContext.runningValues.find(sequence);
    NX_ASSERT(end != m_testContext.runningValues.end());

    std::vector<float> averages;
    std::transform(std::prev(end, m_minBandwidthRequests), end, std::back_inserter(averages),
        [](const auto& elem) { return elem.second.averageBandwidth; });

    NX_VERBOSE(this, "Comparing %1 bandwidths: %2, similarity threshold = %3",
        averages.size(), containerString(averages), kSimilarityThreshold);

    for (auto it = std::next(averages.begin(), 1); it != averages.end(); ++it)
    {
        const auto high = std::max(*averages.begin(), *it);
        const auto low = std::min(*averages.begin(), *it);

        if (!low || !high || low / high < kSimilarityThreshold)
            return std::nullopt;
    }

    return *averages.begin();
}

void UplinkBandwidthTester::onMessageReceived(network::http::Message message)
{
    if (!m_handler) //< If handler is nullptr, test is already completed and handler invoked
        return;

    auto sequence = parseSequence(message);
    if (!sequence)
        return testFailed(SystemError::invalidData, "failed to parse sequence");

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
        if (*sequence == 0)
            return testFailed(SystemError::invalidData, "sequence == 0, should not happen");

        if (currentDuration < kMinTestDuration)
        {
            NX_VERBOSE(this, "currentDuration(%1) < kMinTestDuration(%2)",
                duration_cast<milliseconds>(currentDuration), kMinTestDuration);
            currentDuration = kMinTestDuration;
        }


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
                return testFailed(errorCode, "pipeline failed to send data");

            sendRequest();
        });

    NX_VERBOSE(this, "Sent request %1, totalBytesSent: %2",
        sequence, m_testContext.totalBytesSent);
}

void UplinkBandwidthTester::testComplete(int bytesPerMsec)
{
    if (m_handler)
    {
        m_testContext = TestContext();

        // * 1000 to kilobits per sec, * 8 converts to bits per msec, / 1024 to kilobits per msec
        const auto kilobitsPerSec = ((long long) bytesPerMsec * 1000 * 8) / 1024;

        NX_VERBOSE(this, "Test complete, reporting bytes per msec %1 (%2 Kbps)",
            bytesPerMsec, kilobitsPerSec);

        nx::utils::swapAndCall(m_handler, SystemError::noError, (int) kilobitsPerSec);
    }
}

void UplinkBandwidthTester::testFailed(SystemError::ErrorCode errorCode, const QString& reason)
{
    if (m_handler)
    {
        NX_VERBOSE(this, "Test failed, errorCode: %1: %2", errorCode, reason);

        m_testContext = TestContext();
        nx::utils::swapAndCall(m_handler, errorCode, 0);
    }
}

} // namespace nx::network::cloud::speed_test
