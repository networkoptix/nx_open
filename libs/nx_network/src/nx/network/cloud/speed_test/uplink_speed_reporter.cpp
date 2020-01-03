#include "uplink_speed_reporter.h"

#include <nx/network/aio/scheduler.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/speed_test/uplink_speed_tester_factory.h>
#include <nx/utils/random.h>

namespace nx::network::cloud::speed_test {

namespace {

static constexpr std::chrono::milliseconds kOneAm = std::chrono::hours(1);
static constexpr std::chrono::milliseconds kFourAm = std::chrono::hours(4);
static constexpr std::chrono::milliseconds kOneDay = std::chrono::hours(24);

} // namespace

UplinkSpeedReporter::UplinkSpeedReporter(
    hpm::api::MediatorConnector* mediatorConnector,
    const std::optional<nx::utils::Url>& speedTestUrlMockup,
    std::unique_ptr<nx::network::aio::Scheduler> scheduler):
    m_mediatorConnector(mediatorConnector),
    m_speedTestUrlMockup(speedTestUrlMockup),
    m_scheduler(std::move(scheduler))
{
    m_mediatorConnector->subsribeToSystemCredentialsSet(
        std::bind(
            &UplinkSpeedReporter::onSystemCredentialsSet,
            this,
            std::placeholders::_1),
    &m_systemCredentialsSubscriptionId);

    if (!m_scheduler)
    {
        const auto randomTime = std::chrono::milliseconds(
            nx::utils::random::number(kOneAm.count(), kFourAm.count()));
        m_scheduler = std::make_unique<nx::network::aio::Scheduler>(
            kOneDay,
            std::set{randomTime});
    }
    m_scheduler->bindToAioThread(getAioThread());

    onSystemCredentialsSet(m_mediatorConnector->getSystemCredentials());
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
    if (m_cloudModuleUrlFetcher)
        m_cloudModuleUrlFetcher->bindToAioThread(aioThread);
    if (m_uplinkSpeedTester)
        m_uplinkSpeedTester->bindToAioThread(aioThread);
    if (m_mediatorApiClient)
        m_mediatorApiClient->bindToAioThread(aioThread);
    m_scheduler->bindToAioThread(aioThread);
}

void UplinkSpeedReporter::setAboutToRunSpeedTestHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    QnMutexLocker lock(&m_mutex);
    m_aboutToRunSpeedTestHandler = std::move(handler);
}

void UplinkSpeedReporter::stopTest()
{
    m_testInProgress = false;
    m_peerConnectionSpeed = std::nullopt;
    m_cloudModuleUrlFetcher.reset();
    m_uplinkSpeedTester.reset();
    m_mediatorApiClient.reset();
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

            {
                QnMutexLocker lock(&m_mutex);
                if (m_aboutToRunSpeedTestHandler)
                    m_aboutToRunSpeedTestHandler(!m_testInProgress.load());
            }

            if (m_testInProgress)
            {
                NX_VERBOSE(this, "Speed test already in progress, skipping.");
                return;
            }
            m_testInProgress = true;

            m_cloudModuleUrlFetcher =
                std::make_unique<CloudModuleUrlFetcher>(network::cloud::kSpeedTestModuleName);
            m_cloudModuleUrlFetcher->bindToAioThread(getAioThread());

            {
                QnMutexLocker lock(&m_mutex);
                if (m_speedTestUrlMockup)
                    m_cloudModuleUrlFetcher->setUrl(*m_speedTestUrlMockup);
            }

            NX_VERBOSE(this, "Fetching speed test url...");

            // NOTE: if setUrl has been called, calling setModulesXmlUrl() has no effect.
            // get() will return immediately.
            m_cloudModuleUrlFetcher->setModulesXmlUrl(
                AppInfo::defaultCloudModulesXmlUrl(
                    AppInfo::defaultCloudHostName()));

            m_cloudModuleUrlFetcher->get(
                std::bind(&UplinkSpeedReporter::onFetchSpeedTestUrlComplete, this, _1, _2));
        });
}

void UplinkSpeedReporter::onSystemCredentialsSet(
    std::optional<hpm::api::SystemCredentials> credentials)
{
    NX_VERBOSE(this, "Cloud System Credentials have been set");

    if (!credentials)
        return disable(__func__);

    if (m_cloudModuleUrlFetcher)
        return;

    NX_VERBOSE(this, "Starting scheduler");
    m_scheduler->start(std::bind(&UplinkSpeedReporter::fetchSpeedTestUrl, this));

    // Manually performing speed test because valid system credentials were set.
    fetchSpeedTestUrl();
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

    if (!m_uplinkSpeedTester)
        m_uplinkSpeedTester = UplinkSpeedTesterFactory::instance().create();

    m_uplinkSpeedTester->bindToAioThread(getAioThread());

    m_uplinkSpeedTester->start(
        speedTestUrl,
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

    m_peerConnectionSpeed = hpm::api::PeerConnectionSpeed{
        systemCredentials->serverId.toStdString(),
        systemCredentials->systemId.toStdString(),
        std::move(*connectionSpeed)
    };

    NX_VERBOSE(this, "Fetching Mediator address...");
    m_mediatorConnector->fetchAddress(
        std::bind(&UplinkSpeedReporter::onFetchMediatorAddressComplete, this, _1, _2));
}

void UplinkSpeedReporter::onFetchMediatorAddressComplete(
        http::StatusCode::Value statusCode,
        hpm::api::MediatorAddress mediatorAddress)
{
    NX_VERBOSE(this,
        "Fetched Mediator adress, http status code = %1, mediator address = {%2}",
        http::StatusCode::toString(statusCode), mediatorAddress);

    if (!http::StatusCode::isSuccessCode(statusCode))
    {
        NX_VERBOSE(this, "Failed to fetch mediator address, speed test will not be performed");
        return stopTest();
    }

    if (!m_peerConnectionSpeed)
    {
        NX_ERROR(this, "Speed test was completed, but m_peerConnectionSpeed does not have a value");
        return stopTest();
    }

    if (!m_mediatorApiClient)
    {
        m_mediatorApiClient = std::make_unique<hpm::api::Client>(
            url::Builder(mediatorAddress.tcpUrl).setScheme(http::kUrlSchemeName));
        m_mediatorApiClient->bindToAioThread(getAioThread());
    }

    NX_VERBOSE(this, "Reporting PeerConnectionSpeed %1 to Mediator...", *m_peerConnectionSpeed);

    m_mediatorApiClient->reportUplinkSpeed(
        *m_peerConnectionSpeed,
        [this](auto resultCode)
        {
            NX_VERBOSE(this, "reportUplinkSpeed complete, resultCode = %1", resultCode);
        });
}

} // namespace nx::network::cloud::speed_test
