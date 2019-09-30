#pragma once

#include <optional>

#include <nx/utils/url.h>

#include <nx/network/http/http_types.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/connection_speed.h>

#include "abstract_speed_tester.h"

namespace nx::hpm::api { class Client; }

namespace nx::network::cloud {

class CloudModuleUrlFetcher;

namespace speed_test {

class ScheduledUplinkSpeedTester;

class NX_NETWORK_API UplinkSpeedReporter: public aio::BasicPollable
{
    using base_type = aio::BasicPollable;
public:
    UplinkSpeedReporter(
        hpm::api::MediatorConnector* mediatorConnector,
        const std::optional<nx::utils::Url>& speedTestUrlMockup = std::nullopt);
    ~UplinkSpeedReporter();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Causes cloud_modules.xml lookup of speed test service url to return immediately
     */
    void mockupSpeedTestUrl(const nx::utils::Url& url);

private:
    void onSystemCredentialsSet(std::optional<hpm::api::SystemCredentials> credentials);

    void onFetchSpeedTestUrlComplete(
        http::StatusCode::Value statusCode,
        nx::utils::Url speedTestUrl);

    void onSpeedTestComplete(
        SystemError::ErrorCode errorCode,
        std::optional<hpm::api::ConnectionSpeed> connectionSpeed,
        hpm::api::MediatorAddress mediatorAddress);

    void disable(const char* callingFunc);

private:
    hpm::api::MediatorConnector* m_mediatorConnector;
    utils::SubscriptionId m_systemCredentialsSubscriptionId;
    std::unique_ptr<speed_test::AbstractSpeedTester> m_uplinkSpeedTester;
    std::unique_ptr<hpm::api::Client> m_mediatorApiClient;
    std::unique_ptr<CloudModuleUrlFetcher> m_cloudModuleUrlFetcher;

    QnMutex m_mutex;
    std::optional<nx::utils::Url> m_speedTestUrlMockup;
};

} // namespace speed_test
} // namespace nx::network::cloud
