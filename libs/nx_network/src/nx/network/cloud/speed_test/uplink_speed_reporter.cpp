// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uplink_speed_reporter.h"

#include <nx/network/aio/scheduler.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester_factory.h>
#include <nx/network/cloud/cloud_connect_controller.h>

#include <nx/utils/random.h>

namespace nx::network::cloud::speed_test {

namespace {

static constexpr std::chrono::milliseconds kOneAm = std::chrono::hours(1);
static constexpr std::chrono::milliseconds kFourAm = std::chrono::hours(4);
static constexpr std::chrono::milliseconds kOneDay = std::chrono::hours(24);

} // namespace

UplinkSpeedReporter::UplinkSpeedReporter(
    const nx::utils::Url& cloudModulesXmlUrl,
    hpm::api::MediatorConnector* mediatorConnector,
    std::unique_ptr<nx::network::aio::Scheduler> scheduler,
    const AbstractSpeedTester::Settings& settings)
    :
    m_cloudModulesXmlUrl(cloudModulesXmlUrl),
    m_mediatorConnector(mediatorConnector),
    m_scheduler(std::move(scheduler)),
    m_speedTestSettings(settings)
{
    if (!m_scheduler)
    {
        const auto randomTime = std::chrono::milliseconds(
            nx::utils::random::number(kOneAm.count(), kFourAm.count()));
        m_scheduler = std::make_unique<nx::network::aio::Scheduler>(
            kOneDay,
            std::set{randomTime});
    }
    bindToAioThread(m_mediatorConnector->getAioThread());
}

UplinkSpeedReporter::~UplinkSpeedReporter()
{
    m_mediatorConnector->unsubscribeFromSystemCredentialsSet(
        m_systemCredentialsSubscriptionId);
    pleaseStopSync();
}

void UplinkSpeedReporter::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    disable(__func__);
}

void UplinkSpeedReporter::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_speedTestUrlFetcher)
        m_speedTestUrlFetcher->bindToAioThread(aioThread);
    if (m_uplinkSpeedTester)
        m_uplinkSpeedTester->bindToAioThread(aioThread);
    if (m_mediatorApiClient)
        m_mediatorApiClient->bindToAioThread(aioThread);
    m_scheduler->bindToAioThread(aioThread);
}

void UplinkSpeedReporter::setAboutToRunSpeedTestHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_aboutToRunSpeedTestHandler = std::move(handler);
}

void UplinkSpeedReporter::setFetchMediatorAddressHandler(
    FetchMediatorAddressHandler handler)
{
    m_fetchMediatorAddressHandler = std::move(handler);
}

void UplinkSpeedReporter::setCloudModulesXmlUrl(const nx::utils::Url& url)
{
    m_cloudModulesXmlUrl = url;
}

void UplinkSpeedReporter::start()
{
    m_mediatorConnector->subsribeToSystemCredentialsSet(
        std::bind(
            &UplinkSpeedReporter::onSystemCredentialsSet,
            this,
            std::placeholders::_1),
    &m_systemCredentialsSubscriptionId);
    onSystemCredentialsSet(m_mediatorConnector->getSystemCredentials());
}

void UplinkSpeedReporter::stopTest()
{
    NX_ASSERT(isInSelfAioThread());
    m_speedTestUrlFetcher.reset();
    m_uplinkSpeedTester.reset();
    m_mediatorApiClient.reset();
    m_testInProgress = false;
}

void UplinkSpeedReporter::disable(const char* callingFunc)
{
    NX_VERBOSE(this, "Disabled from %1", callingFunc);
    m_scheduler->pleaseStopSync();
    stopTest();
}

void UplinkSpeedReporter::fetchSpeedTestUrl()
{
    dispatch(
        [this]() mutable
        {
            using namespace std::placeholders;

            if (m_aboutToRunSpeedTestHandler)
                m_aboutToRunSpeedTestHandler(!m_testInProgress.load());

            if (m_testInProgress)
            {
                NX_VERBOSE(this, "Speed test already in progress, skipping.");
                return;
            }
            m_testInProgress = true;

            m_speedTestUrlFetcher =
                std::make_unique<CloudModuleUrlFetcher>(network::cloud::kSpeedTestModuleName);

            NX_VERBOSE(this, "Fetching speed test url from %1...", m_cloudModulesXmlUrl);

            m_speedTestUrlFetcher->setModulesXmlUrl(m_cloudModulesXmlUrl);

            m_speedTestUrlFetcher->get(
                std::bind(&UplinkSpeedReporter::onFetchSpeedTestUrlComplete, this, _1, _2));
        });
}

void UplinkSpeedReporter::onSystemCredentialsSet(
    std::optional<hpm::api::SystemCredentials> credentials)
{
    dispatch(
        [this, credentials = std::move(credentials)]() mutable
        {
            NX_VERBOSE(this, "Cloud system credentials have been set: %1",
                credentials.has_value());

            if (!credentials)
                return disable(__func__);

            if (m_speedTestUrlFetcher)
                return;

            NX_VERBOSE(this, "Starting scheduler");
            m_scheduler->start(std::bind(&UplinkSpeedReporter::fetchSpeedTestUrl, this));

            // Manually performing speed test because valid system credentials were set.
            fetchSpeedTestUrl();
    });
}

void UplinkSpeedReporter::onFetchSpeedTestUrlComplete(
    http::StatusCode::Value statusCode,
    nx::utils::Url speedTestUrl)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "Fetched speedtest url, http status code = %1, speedtest url = %2",
        http::StatusCode::toString(statusCode), speedTestUrl);

    if (!http::StatusCode::isSuccessCode(statusCode) || speedTestUrl.isEmpty())
        return stopTest();

    m_speedTestSettings.url = speedTestUrl;
    if (!m_uplinkSpeedTester)
        m_uplinkSpeedTester = UplinkSpeedTesterFactory::instance().create(m_speedTestSettings);

    m_uplinkSpeedTester->start(
        std::bind(&UplinkSpeedReporter::onSpeedTestComplete, this, _1, _2));
}

void UplinkSpeedReporter::onSpeedTestComplete(
    SystemError::ErrorCode errorCode,
    std::optional<hpm::api::ConnectionSpeed> connectionSpeed)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "Speed test complete, errorCode = %1", SystemError::toString(errorCode));

    if (errorCode != SystemError::noError)
        return stopTest();

    auto systemCredentials = m_mediatorConnector->getSystemCredentials();
    if (!systemCredentials)
    {
        NX_VERBOSE(this, "SystemCredentials were revoked during a speed test, disabling");
        return disable(__func__);
    }

    hpm::api::PeerConnectionSpeed peerConnectionSpeed{
        systemCredentials->serverId,
        systemCredentials->systemId,
        std::move(*connectionSpeed)
    };

    NX_VERBOSE(this, "Fetching Mediator address...");
    m_mediatorConnector->fetchAddress(
        std::bind(&UplinkSpeedReporter::onFetchMediatorAddressComplete, this,
            _1, _2, std::move(peerConnectionSpeed)));
}

void UplinkSpeedReporter::onFetchMediatorAddressComplete(
    http::StatusCode::Value statusCode,
    hpm::api::MediatorAddress mediatorAddress,
    hpm::api::PeerConnectionSpeed peerConnectionSpeed)
{
    NX_VERBOSE(this,
        "Fetched Mediator adress, http status code = %1, mediator address = {%2}",
        http::StatusCode::toString(statusCode), mediatorAddress);

    if (m_fetchMediatorAddressHandler)
        m_fetchMediatorAddressHandler(statusCode, mediatorAddress);

    if (!http::StatusCode::isSuccessCode(statusCode))
    {
        NX_VERBOSE(this, "Failed to fetch mediator address, speed test will not be performed");
        return stopTest();
    }

    if (!m_mediatorApiClient)
    {
        auto url = url::Builder(mediatorAddress.tcpUrl).setScheme(http::kUrlSchemeName);
        const auto systemCredentials = m_mediatorConnector->getSystemCredentials();
        if (systemCredentials)
        {
            url.setUserName(systemCredentials->systemId);
            url.setPassword(systemCredentials->key);
        }

        m_mediatorApiClient = std::make_unique<hpm::api::Client>(url, ssl::kDefaultCertificateCheck);
    }

    NX_VERBOSE(this, "Reporting PeerConnectionSpeed %1 to Mediator...", peerConnectionSpeed);

    m_mediatorApiClient->reportUplinkSpeed(
        peerConnectionSpeed,
        [this](auto resultCode)
        {
            NX_VERBOSE(this, "reportUplinkSpeed complete, resultCode = %1", resultCode);
            stopTest();
        });
}

} // namespace nx::network::cloud::speed_test
