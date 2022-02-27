// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/system_socket.h>
#include <nx/utils/async_operation_guard.h>

namespace nx::network::cloud::speed_test {

using BandwidthCompletionHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, int /*Kbps*/)>;

class NX_NETWORK_API UplinkBandwidthTester:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;
public:
    UplinkBandwidthTester(
        const nx::utils::Url& url,
        const std::chrono::milliseconds& testDuration,
        int minBandwidthRequests,
        const std::chrono::microseconds& pingTime);
    ~UplinkBandwidthTester();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    /**
     * NOTE: handler reports bandwidth result in Kilobits per second iff there is no error.
     */
    void doBandwidthTest(BandwidthCompletionHandler handler);

private:
    std::pair<int/*sequence*/, nx::Buffer> makeRequest();
    std::optional<int> parseSequence(const network::http::Message& message);
    std::optional<int> stopEarlyIfAble(int sequence)const;

    void onMessageReceived(network::http::Message message);

    void sendRequest();

    void testComplete(int bytesPerMsec);
    void testFailed(SystemError::ErrorCode errorCode, const QString& reason);

private:
    struct RunningValue
    {
        int totalBytesSent = 0;
        // in bytes per msec
        float averageBandwidth = 0;

        std::string toString() const;
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
    const nx::utils::Url m_url;
    const std::chrono::milliseconds m_testDuration;
    const int m_minBandwidthRequests;
    const std::chrono::microseconds m_pingTime;
    std::unique_ptr<network::TCPSocket> m_tcpSocket;
    BandwidthCompletionHandler m_handler;
    TestContext m_testContext;
    std::unique_ptr<network::http::AsyncMessagePipeline> m_pipeline;
    nx::utils::AsyncOperationGuard m_asyncGuard;
};

} // namespace nx::network::cloud::speed_test
