#include "uplink_speed_reporter.h"

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>

#include "uplink_speed_tester_factory.h"

namespace nx::network::cloud::speed_test {

UplinkSpeedReporter::UplinkSpeedReporter(
    hpm::api::MediatorConnector* mediatorConnector,
    const std::optional<nx::utils::Url>& speedTestUrlMockup):
    m_mediatorConnector(mediatorConnector),
    m_speedTestUrlMockup(speedTestUrlMockup)
{
    m_systemCredentialsSubscriptionId = m_mediatorConnector->subsribeToSystemCredentialsSet(
        std::bind(
            &UplinkSpeedReporter::onSystemCredentialsSet,
            this,
            std::placeholders::_1));

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
}

void UplinkSpeedReporter::disable(const char* callingFunc)
{
    NX_VERBOSE(this, "Disabled from %1", callingFunc);
    m_cloudModuleUrlFetcher.reset();
    m_uplinkSpeedTester.reset();
    m_mediatorApiClient.reset();
}

void UplinkSpeedReporter::mockupSpeedTestUrl(const nx::utils::Url& url)
{
    QnMutexLocker lock(&m_mutex);
    m_speedTestUrlMockup = url;
    if (m_cloudModuleUrlFetcher)
        m_cloudModuleUrlFetcher->setUrl(url);
}

void UplinkSpeedReporter::onSystemCredentialsSet(
    std::optional<hpm::api::SystemCredentials> credentials)
{
    if (!credentials)
        return disable(__func__);

    if (m_cloudModuleUrlFetcher)
        return;

    dispatch(
        [this]()mutable
        {
            using namespace std::placeholders;

            m_cloudModuleUrlFetcher =
                std::make_unique<CloudModuleUrlFetcher>(network::cloud::kSpeedTestModuleName);

            {
                QnMutexLocker lock(&m_mutex);
                if (m_speedTestUrlMockup)
                    m_cloudModuleUrlFetcher->setUrl(*m_speedTestUrlMockup);
            }

            m_cloudModuleUrlFetcher->get(
                std::bind(&UplinkSpeedReporter::onFetchSpeedTestUrlComplete, this, _1, _2));
        });
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
        return;
    }

    if (!m_uplinkSpeedTester)
        m_uplinkSpeedTester = UplinkSpeedTesterFactory::instance().create();

    auto address = m_mediatorConnector->address();
    if (address)
    {
        m_uplinkSpeedTester->start(
            speedTestUrl,
            std::bind(&UplinkSpeedReporter::onSpeedTestComplete, this, _1, _2, *address));
    }
}

void UplinkSpeedReporter::UplinkSpeedReporter::onSpeedTestComplete(
    SystemError::ErrorCode errorCode,
    std::optional<hpm::api::ConnectionSpeed> connectionSpeed,
    hpm::api::MediatorAddress mediatorAddress)
{
    if (errorCode != SystemError::noError)
    {
        NX_VERBOSE(this, "Speed test failed: %1", SystemError::toString(errorCode));
        return;
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

    m_mediatorApiClient->reportUplinkSpeed(
        hpm::api::PeerConnectionSpeed{
            systemCredentials->serverId.toStdString(),
            systemCredentials->systemId.toStdString(),
            std::move(*connectionSpeed)
        },
        [this](hpm::api::ResultCode resultCode)
        {
            NX_VERBOSE(this, "reportUplinkSpeed() finished with resultCode: %1", resultCode);
        });
}

} // namespace nx::network::cloud::speed_test
