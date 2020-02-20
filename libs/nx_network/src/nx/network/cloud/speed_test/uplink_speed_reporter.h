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

using FetchMediatorAddressHandler =
    nx::utils::MoveOnlyFunc<void(
        http::StatusCode::Value statusCode,
        const hpm::api::MediatorAddress& mediatorAddress)>;

class NX_NETWORK_API UplinkSpeedReporter:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;
public:
    UplinkSpeedReporter(
        const nx::utils::Url& cloudModulesXmlUrl,
        hpm::api::MediatorConnector* mediatorConnector,
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

    void setFetchMediatorAddressHandler(FetchMediatorAddressHandler handler);

private:
    void onSystemCredentialsSet(std::optional<hpm::api::SystemCredentials> credentials);

    void onFetchSpeedTestUrlComplete(
        http::StatusCode::Value statusCode,
        nx::utils::Url speedTestUrl);

    void onSpeedTestComplete(
        SystemError::ErrorCode errorCode,
        std::optional<hpm::api::ConnectionSpeed> connectionSpeed);

    void onFetchMediatorAddressComplete(
        http::StatusCode::Value statusCode,
        hpm::api::MediatorAddress mediatorAddress);

    void stopTest();
    void disable(const char* callingFunc);

    void fetchSpeedTestUrl();

private:
    nx::utils::Url m_cloudModulesXmlUrl;
    hpm::api::MediatorConnector* m_mediatorConnector;
    nx::utils::SubscriptionId m_systemCredentialsSubscriptionId = nx::utils::kInvalidSubscriptionId;
    std::unique_ptr<AbstractSpeedTester> m_uplinkSpeedTester;
    std::unique_ptr<hpm::api::Client> m_mediatorApiClient;
    std::unique_ptr<CloudModuleUrlFetcher> m_speedTestUrlFetcher;
    std::atomic_bool m_testInProgress = false;
    std::optional<hpm::api::PeerConnectionSpeed> m_peerConnectionSpeed;

    QnMutex m_mutex;
    std::unique_ptr<nx::network::aio::Scheduler> m_scheduler;
    nx::utils::MoveOnlyFunc<void(bool /*inProgress*/)> m_aboutToRunSpeedTestHandler;
    FetchMediatorAddressHandler m_fetchMediatorAddressHandler;
};

} // namespace speed_test
} // namespace cloud
} // namespace nx::network
