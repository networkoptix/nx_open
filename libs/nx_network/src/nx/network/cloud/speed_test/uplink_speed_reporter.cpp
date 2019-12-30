#include "uplink_speed_reporter.h"

#include <nx/network/app_info.h>
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
}

void UplinkSpeedReporter::disable(const char* callingFunc)
{
    NX_VERBOSE(this, "Disabled from %1", callingFunc);
    m_cloudModuleUrlFetcher.reset();
    m_uplinkSpeedTester.reset();
    m_mediatorApiClient.reset();
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

            NX_VERBOSE(this, "Fetching speedtest url from cloud_modules.xml...");

            if (m_testInProgress)
            {
                NX_VERBOSE(this, "Speed test already in progress, skipping.");
                return;
            }
            m_testInProgress = true;

            m_cloudModuleUrlFetcher =
                std::make_unique<CloudModuleUrlFetcher>(network::cloud::kSpeedTestModuleName);
            
            const auto cloudModulesUrl = AppInfo::defaultCloudModulesXmlUrl(AppInfo::defaultCloudHostName());
            NX_VERBOSE(this, "Fetching speed test url from cloud_modules.xml at %1", cloudModulesUrl);
            m_cloudModuleUrlFetcher->setModulesXmlUrl(std::move(cloudModulesUrl));

            {
                QnMutexLocker lock(&m_mutex);
                if (m_speedTestUrlMockup)
                    m_cloudModuleUrlFetcher->setUrl(*m_speedTestUrlMockup);
            }

            m_cloudModuleUrlFetcher->get(
                std::bind(&UplinkSpeedReporter::onFetchSpeedTestUrlComplete, this, _1, _2));
        });
}

void UplinkSpeedReporter::onSystemCredentialsSet(
    std::optional<hpm::api::SystemCredentials> credentials)
{   
    if (!credentials)
        return disable(__func__);

    NX_VERBOSE(this, "Cloud System credentials have been set.");

    if (m_cloudModuleUrlFetcher)
        return;

    m_scheduler->start(std::bind(&UplinkSpeedReporter::fetchSpeedTestUrl, this));

    // Manually performing speed test because valid system credentials were set.
    fetchSpeedTestUrl();
}

void UplinkSpeedReporter::onFetchSpeedTestUrlComplete(
    http::StatusCode::Value statusCode,
    nx::utils::Url speedTestUrl)
{
    using namespace std::placeholders;

    if (!http::StatusCode::isSuccessCode(statusCode) || speedTestUrl.isEmpty())
    {
        NX_VERBOSE(this, "Fetching speed test url from cloud_modules.xml failed: %1",
            http::StatusCode::toString(statusCode));
        return stopTest();
    }

    NX_VERBOSE(this, "Fetched speedtest url: %1", speedTestUrl);

    if (!m_uplinkSpeedTester)
        m_uplinkSpeedTester = UplinkSpeedTesterFactory::instance().create();

    auto address = m_mediatorConnector->address();
    if (!address)
    {
        NX_VERBOSE(this, "Mediator address is missing, speed test will not be performed.");
        return stopTest();
    }

    NX_VERBOSE(this, "Starting speed test...");

    m_uplinkSpeedTester->start(
        speedTestUrl,
        std::bind(&UplinkSpeedReporter::onSpeedTestComplete, this, _1, _2, *address));
}

void UplinkSpeedReporter::UplinkSpeedReporter::onSpeedTestComplete(
    SystemError::ErrorCode errorCode,
    std::optional<hpm::api::ConnectionSpeed> connectionSpeed,
    hpm::api::MediatorAddress mediatorAddress)
{
    NX_VERBOSE(this, "Speed test complete: SystemError = %1", SystemError::toString(errorCode));

    if (errorCode != SystemError::noError)
    {
        NX_VERBOSE(this, "Speed test failed: %1", SystemError::toString(errorCode));
        return stopTest();
    }

    if (!m_mediatorApiClient)
    {
        m_mediatorApiClient = std::make_unique<hpm::api::Client>(
            url::Builder(mediatorAddress.tcpUrl).setScheme(http::kUrlSchemeName));
    }

    auto systemCredentials = m_mediatorConnector->getSystemCredentials();
    if (!systemCredentials)
    {
        NX_VERBOSE(this, "SystemCredentials were revoked during a speed test, disabling.");
        return disable(__func__);
    }

    nx::hpm::api::PeerConnectionSpeed peerConnectionSpeed{
        systemCredentials->serverId.toStdString(),
        systemCredentials->systemId.toStdString(),
        std::move(*connectionSpeed)
    };

    NX_VERBOSE(this,
        "Reporting PeerConnectionSpeed %1 to Connection Mediator at %2...",
        peerConnectionSpeed, mediatorAddress.tcpUrl);

    m_mediatorApiClient->reportUplinkSpeed(
        std::move(peerConnectionSpeed),
        [this](hpm::api::ResultCode resultCode)
        {
            NX_VERBOSE(this, "reportUplinkSpeed() finished with resultCode: %1", resultCode);
            stopTest();
        });
}

} // namespace nx::network::cloud::speed_test
