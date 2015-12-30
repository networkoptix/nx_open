/**********************************************************
* Dec 31, 2015
* akolesnikov
***********************************************************/

#include <condition_variable>
#include <mutex>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/stun/udp_server.h>

#include <utils/common/cpp14.h>
#include <utils/common/sync_call.h>


namespace nx {
namespace stun {
namespace test {

class StunUDP
:
    public ::testing::Test
{
public:
    StunUDP()
    {
        using namespace std::placeholders;
        m_messageDispatcher.registerDefaultRequestProcessor(
            std::bind(&StunUDP::onMessage, this, _1, _2));

        addServer();
    }

    ~StunUDP()
    {
    }

    /** launches another server */
    void addServer()
    {
        m_udpServers.emplace_back(std::make_unique<UDPServer>(m_messageDispatcher));
        assert(m_udpServers.back()->listen());
    }

    /** returns endpoint of any server */
    SocketAddress anyServerEndpoint() const
    {
        return SocketAddress(
            HostAddress::localhost,
            m_udpServers[rand() % m_udpServers.size()]->address().port);
    }

    std::vector<SocketAddress> allServersEndpoints() const
    {
        std::vector<SocketAddress> endpoints;
        endpoints.reserve(m_udpServers.size());
        for (const auto& udpServer: m_udpServers)
        {
            endpoints.push_back(
                SocketAddress(
                    HostAddress::localhost,
                    udpServer->address().port));
        }

        return endpoints;
    }

private:
    MessageDispatcher m_messageDispatcher;
    std::vector<std::unique_ptr<UDPServer>> m_udpServers;

    void onMessage(
        std::shared_ptr< AbstractServerConnection > connection,
        stun::Message message)
    {
        nx::stun::Message response(
            stun::Header(
                nx::stun::MessageClass::successResponse,
                nx::stun::bindingMethod,
                message.header.transactionId));
        connection->sendMessage(std::move(response), nullptr);
    }
};

TEST_F(StunUDP, server_reports_port)
{
    ASSERT_NE(0, anyServerEndpoint().port);
}

/** Common test.
    Sending request to multiple servers, receiving responses
*/
TEST_F(StunUDP, client_test_sync)
{
    static const int REQUESTS_TO_SEND = 100;
    static const int LOCAL_SERVERS_COUNT = 10;

    for (int i = 1; i < LOCAL_SERVERS_COUNT; ++i)
        addServer();

    UDPClient client;
    for (int i = 0; i < REQUESTS_TO_SEND; ++i)
    {
        nx::stun::Message requestMessage(
            stun::Header(
                nx::stun::MessageClass::request,
                nx::stun::bindingMethod));

        SystemError::ErrorCode errorCode = SystemError::noError;
        nx::stun::Message response;
        std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
            std::bind(
                &UDPClient::sendRequest,
                &client,
                anyServerEndpoint(),
                requestMessage,
                std::placeholders::_1));
        ASSERT_EQ(SystemError::noError, errorCode);
        ASSERT_EQ(requestMessage.header.transactionId, response.header.transactionId);
    }

    client.pleaseStopSync();
}

TEST_F(StunUDP, client_test_async)
{
    static const int kRequestToSendCount = 1000;
    static const int kLocalServersCount = 10;
    static const std::chrono::seconds kMaxResponsesWaitTime(3);

    for (int i = 1; i < kLocalServersCount; ++i)
        addServer();

    UDPClient client;
    std::mutex mutex;
    std::condition_variable cond;
    std::set<nx::String> expectedTransactionIDs;

    for (int i = 0; i < kRequestToSendCount; ++i)
    {
        nx::stun::Message requestMessage(
            stun::Header(
                nx::stun::MessageClass::request,
                nx::stun::bindingMethod));

        {
            std::unique_lock<std::mutex> lk(mutex);
            expectedTransactionIDs.insert(requestMessage.header.transactionId);
        }
        client.sendRequest(
            anyServerEndpoint(),
            std::move(requestMessage),
            [&mutex, &cond, &expectedTransactionIDs](
                SystemError::ErrorCode errorCode,
                Message response)
        {
            ASSERT_EQ(SystemError::noError, errorCode);
            std::unique_lock<std::mutex> lk(mutex);
            ASSERT_EQ(1, expectedTransactionIDs.erase(response.header.transactionId));
            if (expectedTransactionIDs.empty())
                cond.notify_all();
        });
    }

    {
        std::unique_lock<std::mutex> lk(mutex);
        cond.wait_for(
            lk,
            kMaxResponsesWaitTime,
            [&expectedTransactionIDs]()->bool {return expectedTransactionIDs.empty();});
        ASSERT_TRUE(expectedTransactionIDs.empty());
    }

    client.pleaseStopSync();
}

/** Testing request retransmission by client.
    - sending request
    - ignoring request on server
    - waiting for retransmission
    - server sends response
    - checking that client received response
*/
TEST_F(StunUDP, client_retransmits_general)
{
}

/** Checking that client reports failure after sending maximum retransmissions allowed.
    - sending request
    - ignoring all request on server
    - counting retransmission issued by a client
    - waiting for client to report failure
*/
TEST_F(StunUDP, client_retransmits_max_retransmits)
{
}

/** Checking that client reports error to waiters if removed before receiving response */
TEST_F(StunUDP, client_cancellation)
{
}

}   //test
}   //stun
}   //nx
