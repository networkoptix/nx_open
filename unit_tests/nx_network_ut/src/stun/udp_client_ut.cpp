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
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <utils/common/cpp14.h>
#include <utils/common/string.h>
#include <utils/common/sync_call.h>


namespace nx {
namespace stun {
namespace test {

class StunUDP
:
    public ::testing::Test
{
public:
    static const QByteArray kServerResponseNonce;

    StunUDP()
    :
        m_messagesToIgnore(0),
        m_totalMessagesReceived(0)
    {
        using namespace std::placeholders;
        m_messageDispatcher.registerDefaultRequestProcessor(
            std::bind(&StunUDP::onMessage, this, _1, _2));

        addServer();
    }

    ~StunUDP()
    {
        for (const auto& server: m_udpServers)
            server->pleaseStopSync();
    }

    /** launches another server */
    void addServer()
    {
        m_udpServers.emplace_back(std::make_unique<UDPServer>(&m_messageDispatcher));
        if (!m_udpServers.back()->listen())
        {
            NX_ASSERT(false);
        }
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

    void ignoreNextMessage(size_t messagesToIgnore = 1)
    {
        m_messagesToIgnore += messagesToIgnore;
    }

    size_t totalMessagesReceived() const
    {
        return m_totalMessagesReceived;
    }

private:
    MessageDispatcher m_messageDispatcher;
    std::vector<std::unique_ptr<UDPServer>> m_udpServers;
    size_t m_messagesToIgnore;
    size_t m_totalMessagesReceived;

    void onMessage(
        std::shared_ptr< AbstractServerConnection > connection,
        stun::Message message)
    {
        ++m_totalMessagesReceived;

        if (m_messagesToIgnore > 0)
        {
            //ignoring message
            --m_messagesToIgnore;
            return;
        }

        nx::stun::Message response(
            stun::Header(
                nx::stun::MessageClass::successResponse,
                nx::stun::bindingMethod,
                message.header.transactionId));
        response.newAttribute<stun::attrs::Nonce>(kServerResponseNonce);
        connection->sendMessage(std::move(response), nullptr);
    }
};

const QByteArray StunUDP::kServerResponseNonce("correctServerResponse");



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
                &UDPClient::sendRequestTo,
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
        client.sendRequestTo(
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
    ignoreNextMessage();

    UDPClient client;
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::stun::Message response;
    std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
        std::bind(
            &UDPClient::sendRequestTo,
            &client,
            anyServerEndpoint(),
            requestMessage,
            std::placeholders::_1));
    ASSERT_EQ(SystemError::noError, errorCode);
    ASSERT_EQ(requestMessage.header.transactionId, response.header.transactionId);
    ASSERT_EQ(2, totalMessagesReceived());

    client.pleaseStopSync();
}

/** Checking that client reports failure after sending maximum retransmissions allowed.
    - sending request
    - ignoring all request on server
    - counting retransmission issued by a client
    - waiting for client to report failure
*/
TEST_F(StunUDP, client_retransmits_max_retransmits)
{
    const int MAX_RETRANSMISSIONS = 5;

    ignoreNextMessage(MAX_RETRANSMISSIONS+1);

    UDPClient client;
    client.setRetransmissionTimeOut(std::chrono::milliseconds(100));
    client.setMaxRetransmissions(MAX_RETRANSMISSIONS);
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::stun::Message response;
    std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
        std::bind(
            &UDPClient::sendRequestTo,
            &client,
            anyServerEndpoint(),
            requestMessage,
            std::placeholders::_1));
    ASSERT_EQ(SystemError::timedOut, errorCode);
    ASSERT_EQ(MAX_RETRANSMISSIONS+1, totalMessagesReceived());

    client.pleaseStopSync();
}

/** Checking that client reports error to waiters if removed before receiving response */
TEST_F(StunUDP, client_cancellation)
{
    const int REQUESTS_TO_SEND = 3;

    UDPClient client;
    client.setRetransmissionTimeOut(std::chrono::seconds(100));
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    ignoreNextMessage(REQUESTS_TO_SEND);

    int errorsReported = 0;
    auto completionHandler = 
        [&errorsReported](
            SystemError::ErrorCode errorCode,
            nx::stun::Message /*response*/)
        {
            NX_ASSERT(errorCode == SystemError::interrupted);
            ++errorsReported;
        };

    for (int i = 0; i < REQUESTS_TO_SEND; ++i)
    {
        requestMessage.header.transactionId = Header::makeTransactionId();
        client.sendRequestTo(
            anyServerEndpoint(),
            requestMessage,
            completionHandler);
    }

    client.pleaseStopSync();

    ASSERT_EQ(REQUESTS_TO_SEND, errorsReported);
}

/** testing that client accepts response from server endpoint only */
TEST_F(StunUDP, client_response_injection)
{
    addServer();

    const auto serverEndpoint = anyServerEndpoint();

    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    //ignoring messages so that UDP client uses retransmits
    ignoreNextMessage(3);

    QnMutex mtx;
    QnWaitCondition cond;
    boost::optional<Message> response;
    SystemError::ErrorCode responseErrorCode = SystemError::noError;

    UDPClient client;
    ASSERT_TRUE(client.bind(SocketAddress(HostAddress::localhost, 0)));
    client.sendRequestTo(
        SocketAddress("localhost", serverEndpoint.port),
        //serverEndpoint,
        requestMessage,
        [&mtx, &cond, &response, &responseErrorCode](
            SystemError::ErrorCode errorCode,
            Message msg)
        {
            QnMutexLocker lk(&mtx);
            responseErrorCode = errorCode;
            response = std::move(msg);
            cond.wakeAll();
        });

    //TODO #ak waiting for message to be sent
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //sending unexpected responses and junk to the client
    auto udpSocket = SocketFactory::createDatagramSocket();
    ASSERT_TRUE(
        udpSocket->setDestAddr(
            SocketAddress(
                HostAddress::localhost,
                client.localAddress().port)));

    const int kPacketsToInject = 20;
    for (int i = 0; i < kPacketsToInject; ++i)
    {
        if ((i == 0) || (rand() % 5 == 0))
        {
            nx::stun::Message injectedResponseMessage(
                stun::Header(
                    nx::stun::MessageClass::successResponse,
                    nx::stun::bindingMethod,
                    requestMessage.header.transactionId));
            injectedResponseMessage.newAttribute<stun::attrs::Nonce>("bad");
            const auto serializedMessage = MessageSerializer::serialized(injectedResponseMessage);
            ASSERT_EQ(serializedMessage.size(), udpSocket->send(serializedMessage)) 
                << SystemError::getLastOSErrorText().toStdString();
        }
        else
        {
            const auto buf = generateRandomName(100 + (rand() % 300));
            ASSERT_EQ(buf.size(), udpSocket->send(buf))
                << SystemError::getLastOSErrorText().toStdString();
        }
    }

    {
        QnMutexLocker lk(&mtx);
        while (!response)
            cond.wait(lk.mutex());
    }

    ASSERT_EQ(SystemError::noError, responseErrorCode);
    const auto* nonceAttr = response->getAttribute<stun::attrs::Nonce>();
    ASSERT_NE(nullptr, nonceAttr);
    ASSERT_EQ(kServerResponseNonce, nonceAttr->getString());

    client.pleaseStopSync();
}

}   //test
}   //stun
}   //nx
