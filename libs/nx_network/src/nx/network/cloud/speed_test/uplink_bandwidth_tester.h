#pragma once

#include <queue>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/system_socket.h>

namespace nx::network::cloud::speed_test {

static constexpr float kBytesPerMsecToMegabitsPerSec = 1000.0F / 1024 / 1024 * 8;

using BandwidthCompletionHandler =
	nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, int /*bytesPerSecond*/)>;

class NX_NETWORK_API UplinkBandwidthTester:
	public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;
public:
    UplinkBandwidthTester(
		const nx::utils::Url& url,
		const std::chrono::milliseconds& testDuration,
		const std::chrono::microseconds& pingTime);
	~UplinkBandwidthTester();

	virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
	virtual void stopWhileInAioThread() override;

	void doBandwidthTest(BandwidthCompletionHandler handler);

private:
    std::pair<int/*sequence*/, nx::Buffer> makeRequest();
	std::optional<int> parseSequence(const network::http::Message& message);
	std::optional<int> stopEarlyIfAble(int sequence)const;

    void onMessageReceived(network::http::Message message);

	void sendRequest();

	void testComplete(int bandwidth);
	void testFailed(SystemError::ErrorCode errorCode);

private:
    struct RunningValue
    {
        int totalBytesSent = 0;
        float averageBandwidth = 0;
    };

	struct TestContext
	{
		std::chrono::system_clock::time_point startTime;
		bool sendRequests = false;
		int sequence = -1;
		QByteArray payload;
		int totalBytesSent = 0;
		std::map<int /*sequence*/, RunningValue> runningValues;
	};

private:
    nx::utils::Url m_url;
	std::chrono::milliseconds m_testDuration;
	std::chrono::microseconds m_pingTime;
    std::unique_ptr<network::http::AsyncMessagePipeline> m_pipeline;
    std::unique_ptr<network::TCPSocket> m_tcpSocket;
    BandwidthCompletionHandler m_handler;
	TestContext m_testContext;
};

} // namespace nx::network::cloud::speed_test