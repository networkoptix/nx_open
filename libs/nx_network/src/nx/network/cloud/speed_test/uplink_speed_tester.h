#pragma once

#include <nx/network/cloud/mediator/api/connection_speed.h>
#include <nx/network/http/http_async_client.h>
#include "uplink_bandwidth_tester.h"

namespace nx::network::cloud::speed_test {

using CompletionHandler =
    nx::utils::MoveOnlyFunc<void(
		SystemError::ErrorCode,
		std::optional<nx::hpm::api::ConnectionSpeed>)>;

class NX_NETWORK_API UplinkSpeedTester: public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
	~UplinkSpeedTester();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
	virtual void stopWhileInAioThread() override;

    void start(const nx::utils::Url& speedTestUrl, CompletionHandler handler);

private:
	void startBandwidthTest(const std::chrono::microseconds& pingTime);

    void setupPingTest();
    void startPingTest();
    void sendPing();

    void emitTestResult(
		SystemError::ErrorCode errorCode,
		std::optional<nx::hpm::api::ConnectionSpeed> result);

private:
    struct PingTestContext
    {
		static constexpr int kMaxPingRequests = 20;

        std::chrono::milliseconds startTime;
		std::chrono::microseconds requestSentTime;
		std::chrono::microseconds totalRoundTripTime;
		int totalRoundTrips = 0;
    };

private:
    nx::utils::Url m_url;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::unique_ptr<UplinkBandwidthTester> m_bandwidthTester;
    PingTestContext m_testContext;

    CompletionHandler m_handler;
};

//-------------------------------------------------------------------------------------------------
// ScheduledUplinkSpeedTester

class NX_NETWORK_API ScheduledUplinkSpeedTester: public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    /**
     * @param testSchedule is the set of local times when each test should be sheduled
     */
	ScheduledUplinkSpeedTester(
		const std::set<std::chrono::milliseconds>& testSchedule = {
			std::chrono::hours(3) /*3 a.m.*/ });
	~ScheduledUplinkSpeedTester();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
	virtual void stopWhileInAioThread() override;

    void start(const nx::utils::Url& speedTestUrl, CompletionHandler handler);

	std::chrono::milliseconds waitTimeBeforeNextTest() const;

private:
    void scheduleNextTest(const std::chrono::milliseconds& wait);

private:
	std::set<std::chrono::milliseconds> m_testSchedule;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
	std::unique_ptr<UplinkSpeedTester> m_speedTester;
	nx::utils::Url m_url;
    CompletionHandler m_handler;
};

} // namespace nx::network::cloud::speed_test