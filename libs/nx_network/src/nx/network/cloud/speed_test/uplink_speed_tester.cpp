// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uplink_speed_tester.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/time.h>

#include "http_api_paths.h"

namespace nx::network::cloud::speed_test {

using namespace std::chrono;
using namespace nx::hpm::api;

UplinkSpeedTester::UplinkSpeedTester(const Settings& settings)
    :
    m_settings(settings)
{
}

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

void UplinkSpeedTester::start(CompletionHandler handler)
{
    m_handler = std::move(handler);
    dispatch(
        [this]() mutable
        {
            m_httpClient = std::make_unique<network::http::AsyncClient>(
                network::ssl::kDefaultCertificateCheck);
            startPingTest();
        });
}

void UplinkSpeedTester::startBandwidthTest(const microseconds& pingTime)
{
    NX_VERBOSE(this, "Starting bandwidth test...");
    m_bandwidthTester =
        std::make_unique<UplinkBandwidthTester>(
            m_settings.url,
            m_settings.testDuration,
            m_settings.minBandwidthRequests,
            pingTime);
    m_bandwidthTester->bindToAioThread(getAioThread());
    m_bandwidthTester->doBandwidthTest(
        [this, pingTime](SystemError::ErrorCode errorCode, int bandwidth)
        {
            NX_VERBOSE(this,
                "Bandwidth test complete, SystemError = %1",
                SystemError::toString(errorCode));
            if (errorCode != SystemError::noError)
                return emitTestResult(errorCode, std::nullopt);

            emitTestResult(
                SystemError::noError,
                ConnectionSpeed{duration_cast<milliseconds>(pingTime), bandwidth});
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

            NX_VERBOSE(this,
            "current ping: %1, total ping time: %2, total pings: %3, average ping: %4",
                pingTime, m_testContext.totalPingTime, m_testContext.totalPings, averagePingTime);

            if (now - m_testContext.startTime < m_settings.testDuration
                && m_testContext.totalPings < m_settings.maxPingRequests)
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
    m_httpClient->doGet(url::Builder(m_settings.url).setPath(http::kApiPath));
    m_testContext.requestSentTime = nx::utils::utcTime();
}

void UplinkSpeedTester::emitTestResult(
    SystemError::ErrorCode errorCode,
    std::optional<ConnectionSpeed> result)
{
    QString resultStr = result
        ? nx::format("{pingTime: %1, bandwidth: %2 Kbps}")
        .args(result->pingTime, result->bandwidth)
        : "none";

    NX_VERBOSE(this, "Test complete, reporting system error: %1 and speed test result: %2",
        SystemError::toString(errorCode), resultStr);

    if (m_handler)
        nx::utils::swapAndCall(m_handler, errorCode, std::move(result));
}


} // namespace nx::network::cloud::speed_test
