#pragma once

#include "abstract_speed_tester.h"

#include <nx/network/cloud/mediator/api/connection_speed.h>
#include <nx/network/http/http_async_client.h>
#include "uplink_bandwidth_tester.h"

namespace nx::network::cloud::speed_test {

class NX_NETWORK_API UplinkSpeedTester:
    public AbstractSpeedTester
{
    using base_type = aio::BasicPollable;

public:
	~UplinkSpeedTester();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
	virtual void stopWhileInAioThread() override;

    virtual void start(const nx::utils::Url& url, CompletionHandler handler) override;

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
        std::chrono::system_clock::time_point startTime;
		std::chrono::system_clock::time_point requestSentTime;
		std::chrono::microseconds totalPingTime;
		int totalPings = 0;
    };

private:
    nx::utils::Url m_url;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::unique_ptr<UplinkBandwidthTester> m_bandwidthTester;
    PingTestContext m_testContext;

    CompletionHandler m_handler;
};

} // namespace nx::network::cloud::speed_test