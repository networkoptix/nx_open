#pragma once

#include <optional>

#include <nx/utils/subscription.h>
#include <nx/utils/url.h>

#include <nx/network/aio/scheduler.h>
#include <nx/network/http/http_types.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/connection_speed.h>

namespace nx::hpm::api { class Client; }

namespace nx::network {

namespace aio { class Scheduler; };

namespace cloud {

class CloudModuleUrlFetcher;

namespace speed_test {

class AbstractSpeedTester;

class NX_NETWORK_API UplinkSpeedReporter:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;
public:
    UplinkSpeedReporter(
        hpm::api::MediatorConnector* mediatorConnector,
        const std::optional<nx::utils::Url>& speedTestUrlMockup = std::nullopt,
        std::unique_ptr<nx::network::aio::Scheduler> scheduler = nullptr);
    ~UplinkSpeedReporter();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Invoked right before the SpeedTestService url is fetched.
     * NOTE: There may already be a test in progress when this handler is invoked. In that case,
     * aboutToStart will be false, otherwise true.
     * NOTE: handler is invoked before each attempt to run the speed test.
     * It is not discarded after invokation.
     */
    void setAboutToRunSpeedTestHandler(
        nx::utils::MoveOnlyFunc<void(bool /*aboutToStart*/)> handler);

private:
    void onSystemCredentialsSet(std::optional<hpm::api::SystemCredentials> credentials);

    void onFetchSpeedTestUrlComplete(
        http::StatusCode::Value statusCode,
        nx::utils::Url speedTestUrl);

    void onSpeedTestComplete(
        SystemError::ErrorCode errorCode,
        std::optional<hpm::api::ConnectionSpeed> connectionSpeed,
        hpm::api::MediatorAddress mediatorAddress);

    void stopTest();
    void disable(const char* callingFunc);

    void fetchSpeedTestUrl();

private:
    hpm::api::MediatorConnector* m_mediatorConnector;
    utils::SubscriptionId m_systemCredentialsSubscriptionId = nx::utils::kInvalidSubscriptionId;
    std::unique_ptr<AbstractSpeedTester> m_uplinkSpeedTester;
    std::unique_ptr<hpm::api::Client> m_mediatorApiClient;
    std::unique_ptr<CloudModuleUrlFetcher> m_cloudModuleUrlFetcher;
    std::atomic_bool m_testInProgress = false;

    QnMutex m_mutex;
    std::optional<nx::utils::Url> m_speedTestUrlMockup;
    std::unique_ptr<nx::network::aio::Scheduler> m_scheduler;
    nx::utils::MoveOnlyFunc<void(bool /*inProgress*/)> m_aboutToRunSpeedTestHandler;
};

} // namespace speed_test
} // namespace cloud
} // namespace nx::network
