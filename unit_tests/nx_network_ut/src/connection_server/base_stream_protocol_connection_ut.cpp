#include <memory>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace server {
namespace test {

namespace {

class AsyncChannelToStreamSocketAdapter:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    AsyncChannelToStreamSocketAdapter(aio::AbstractAsyncChannel* asyncChannel):
        base_type(&m_tcpSocket),
        m_asyncChannel(asyncChannel)
    {
        m_asyncChannel->bindToAioThread(getAioThread());
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_asyncChannel->bindToAioThread(aioThread);
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        m_asyncChannel->readSomeAsync(buffer, std::move(handler));
    }

private:
    TCPSocket m_tcpSocket;
    aio::AbstractAsyncChannel* m_asyncChannel;
};

struct HttpMessageTestData
{
    nx::Buffer fullMessage;
    nx::Buffer messageBody;
};

//-------------------------------------------------------------------------------------------------

struct HttpMessageWithIncompleteInfiniteBody: HttpMessageTestData
{
    HttpMessageWithIncompleteInfiniteBody()
    {
        fullMessage = 
            "HTTP/1.1 200 OK\r\n"
            "Server: Network Optix\r\n"
            "Connection: close\r\n"
            "Content-Type: video/mp2t\r\n"
            "\r\n"
            "Here goes mpeg2-ts video.";
        messageBody = "Here goes mpeg2-ts video.";
    }
};
static const HttpMessageWithIncompleteInfiniteBody kHttpMessageWithIncompleteInfiniteBody;

//-------------------------------------------------------------------------------------------------

struct HttpMessageWithFiniteBody: HttpMessageTestData
{
    HttpMessageWithFiniteBody()
    {
        fullMessage = 
            "HTTP/1.1 200 OK\r\n"
            "Server: Network Optix\r\n"
            "Connection: close\r\n"
            "Content-Type: video/mp2t\r\n"
            "Content-Length: 25\r\n"
            "\r\n"
            "Here goes mpeg2-ts video.";

        messageBody = "Here goes mpeg2-ts video.";
    }
};
static const HttpMessageWithFiniteBody kHttpMessageWithFiniteBody;

//-------------------------------------------------------------------------------------------------

struct HttpMessageWithoutBody: HttpMessageTestData
{
    HttpMessageWithoutBody()
    {
        fullMessage = 
            "HTTP/1.1 204 No Content\r\n"
            "Server: Network Optix\r\n"
            "Connection: close\r\n"
            "Content-Type: video/mp2t\r\n"
            "\r\n";
    }
};
static const HttpMessageWithoutBody kHttpMessageWithoutBody;

//-------------------------------------------------------------------------------------------------

using TestHttpConnection = nx_http::AsyncMessagePipeline;

} // namespace

/**
 * Testing BaseStreamProtocolConnection using AsyncMessagePipeline since 
 *   Http already has convenient infrastructure around it.
 */
class BaseStreamProtocolConnection:
    public ::testing::Test,
    public StreamConnectionHolder<TestHttpConnection>
{
public:
    BaseStreamProtocolConnection():
        m_asyncChannel(
            &m_input,
            nullptr,
            aio::test::AsyncChannel::InputDepletionPolicy::retry)
    {
        using namespace std::placeholders;

        m_connection = std::make_unique<TestHttpConnection>(
            this,
            std::make_unique<AsyncChannelToStreamSocketAdapter>(&m_asyncChannel));
        m_connection->setMessageHandler(
            std::bind(&BaseStreamProtocolConnection::saveMessage, this, _1));
        m_connection->setOnSomeMessageBodyAvailable(
            std::bind(&BaseStreamProtocolConnection::saveSomeBody, this, _1));
        m_connection->startReadingConnection();
    }

    ~BaseStreamProtocolConnection()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
        m_asyncChannel.pleaseStopSync();
    }

protected:
    void whenReadMessageWithIncompleteBody()
    {
        m_messagesSent.push_back(kHttpMessageWithIncompleteInfiniteBody);
        m_expectedBody = kHttpMessageWithIncompleteInfiniteBody.messageBody;

        sendMessages();
    }

    void whenReadMessageWithFiniteBody()
    {
        m_messagesSent.push_back(kHttpMessageWithFiniteBody);
        m_expectedBody = kHttpMessageWithFiniteBody.messageBody;

        sendMessagesInFragments();
    }

    void whenReadMessageWithoutBody()
    {
        m_messagesSent.push_back(kHttpMessageWithoutBody);
        m_expectedBody = kHttpMessageWithoutBody.messageBody;

        sendMessages();
    }

    void whenReadMultipleMessages()
    {
        m_messagesSent.push_back(kHttpMessageWithoutBody);
        m_messagesSent.push_back(kHttpMessageWithFiniteBody);
        m_messagesSent.push_back(kHttpMessageWithFiniteBody);
        m_messagesSent.push_back(kHttpMessageWithoutBody);

        sendMessages();
    }

    void thenMessageIsReported()
    {
        //m_receivedMessageQueue.pop();
        thenEveryMessageIsReceived();
    }
    
    void thenMessageBodyIsReported()
    {
        m_receivedMsgBody.waitForReceivedDataToMatch(m_expectedBody);
    }

    void thenEveryMessageIsReceived()
    {
        for (const auto& message: m_messagesSent)
        {
            auto messageReceived = m_receivedMessageQueue.pop();
            for (;;)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    if (message.fullMessage == messageReceived->toString())
                        break;
                }

                std::this_thread::yield();
            }
        }
    }

    virtual void saveMessage(nx_http::Message message)
    {
        auto messageToSave = std::make_unique<nx_http::Message>(std::move(message));
        m_prevMessageReceived = messageToSave.get();
        m_receivedMessageQueue.push(std::move(messageToSave));
    }

    TestHttpConnection& connection()
    {
        return *m_connection;
    }

    nx::Buffer expectedBody() const
    {
        return m_expectedBody;
    }

private:
    nx::utils::bstream::ReflectingPipeline m_input;
    aio::test::AsyncChannel m_asyncChannel;
    std::unique_ptr<TestHttpConnection> m_connection;
    nx::utils::SyncQueue<std::unique_ptr<nx_http::Message>> m_receivedMessageQueue;
    nx::utils::bstream::test::NotifyingOutput m_receivedMsgBody;
    nx::Buffer m_expectedBody;
    std::vector<HttpMessageTestData> m_messagesSent;
    nx_http::Message* m_prevMessageReceived = nullptr;
    QnMutex m_mutex;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        TestHttpConnection* connection) override
    {
        ASSERT_EQ(m_connection.get(), connection);
    }

    void sendMessages()
    {
        for (const auto& message: m_messagesSent)
            m_input.write(message.fullMessage.data(), message.fullMessage.size());
    }

    void sendMessagesInFragments()
    {
        const int step = 1;

        for (const auto& message: m_messagesSent)
        {
            for (int pos = 0;
                pos < message.fullMessage.size();
                pos += step)
            {
                const auto bytesToRead =
                    std::min<int>(step, message.fullMessage.size() - pos);
                m_input.write(message.fullMessage.data() + pos, bytesToRead);
            }
        }
    }

    void saveSomeBody(nx::Buffer bodyBuffer)
    {
        QnMutexLocker lock(&m_mutex);

        m_prevMessageReceived->response->messageBody.append(bodyBuffer);
        m_receivedMsgBody.write(bodyBuffer.constData(), bodyBuffer.size());
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(BaseStreamProtocolConnection, message_is_reported_before_message_body)
{
    whenReadMessageWithIncompleteBody();
    thenMessageIsReported();
}

TEST_F(BaseStreamProtocolConnection, message_body_is_reported)
{
    whenReadMessageWithIncompleteBody();
    thenMessageBodyIsReported();
}

TEST_F(BaseStreamProtocolConnection, full_message_body_is_reported)
{
    whenReadMessageWithFiniteBody();
    thenMessageBodyIsReported();
}

TEST_F(BaseStreamProtocolConnection, message_without_message_body)
{
    whenReadMessageWithoutBody();
    thenMessageIsReported();
}

TEST_F(BaseStreamProtocolConnection, receiving_multiple_messages_over_same_connection)
{
    whenReadMultipleMessages();
    thenEveryMessageIsReceived();
}

// TEST_F(BaseStreamProtocolConnection, message_parse_error)

//-------------------------------------------------------------------------------------------------

class BaseStreamProtocolConnectionTakeSocket:
    public BaseStreamProtocolConnection
{
    using base_type = BaseStreamProtocolConnection;

protected:
    void whenTakenSocketInProcessMessageHandler()
    {
        while (!m_streamSocket)
            std::this_thread::yield();
    }

    void thenMessageBodyCanBeReadFromSocket()
    {
        using namespace std::placeholders;

        m_readBuffer.reserve(expectedBody().size());

        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            std::bind(&BaseStreamProtocolConnectionTakeSocket::onSomeBytesRead, this, _1, _2));

        m_done.get_future().wait();
    }

    virtual void saveMessage(nx_http::Message message)
    {
        base_type::saveMessage(message);

        m_streamSocket = connection().takeSocket();
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::Buffer m_readBuffer;
    nx::utils::promise<void> m_done;

    void onSomeBytesRead(
        SystemError::ErrorCode sysErrorCode, std::size_t /*bytesRead*/)
    {
        using namespace std::placeholders;

        ASSERT_EQ(SystemError::noError, sysErrorCode);

        if (m_readBuffer == expectedBody())
        {
            m_done.set_value();
            return;
        }

        ASSERT_TRUE(expectedBody().startsWith(m_readBuffer));

        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            std::bind(&BaseStreamProtocolConnectionTakeSocket::onSomeBytesRead, this, _1, _2));
    }
};

TEST_F(BaseStreamProtocolConnectionTakeSocket, taking_socket_does_not_lose_data)
{
    whenReadMessageWithIncompleteBody();
    whenTakenSocketInProcessMessageHandler();
    thenMessageBodyCanBeReadFromSocket();
}

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
