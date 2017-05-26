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
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
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

using TestHttpConnection = 
    nx::network::server::BaseStreamProtocolConnectionEmbeddable<
        nx_http::Message,
        nx_http::MessageParser,
        nx_http::MessageSerializer>;

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
        m_input.write(
            kHttpMessageWithIncompleteInfiniteBody.fullMessage.data(),
            kHttpMessageWithIncompleteInfiniteBody.fullMessage.size());
        m_expectedBody = kHttpMessageWithIncompleteInfiniteBody.messageBody;
    }

    void whenReadMessageWithFiniteBody()
    {
        const std::size_t step = 1;

        for (std::size_t pos = 0;
            pos < kHttpMessageWithFiniteBody.fullMessage.size();
            pos += step)
        {
            const auto bytesToRead = 
                std::min<std::size_t>(
                    step,
                    kHttpMessageWithFiniteBody.fullMessage.size() - pos);
            m_input.write(
                kHttpMessageWithFiniteBody.fullMessage.data() + pos,
                bytesToRead);
        }

        m_expectedBody = kHttpMessageWithFiniteBody.messageBody;
    }

    void whenReadMessageWithoutBody()
    {
        m_input.write(
            kHttpMessageWithoutBody.fullMessage.data(),
            kHttpMessageWithoutBody.fullMessage.size());
        m_expectedBody = kHttpMessageWithoutBody.messageBody;
    }

    void whenReadMultipleMessages()
    {
        m_messagesSent.push_back(kHttpMessageWithoutBody);
        m_messagesSent.push_back(kHttpMessageWithFiniteBody);
        m_messagesSent.push_back(kHttpMessageWithFiniteBody);
        m_messagesSent.push_back(kHttpMessageWithoutBody);

        for (const auto& message: m_messagesSent)
            m_input.write(message.fullMessage.data(), message.fullMessage.size());
    }

    void thenMessageIsReported()
    {
        m_receivedMessageQueue.pop();
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

    void saveMessage(nx_http::Message message)
    {
        auto messageToSave = std::make_unique<nx_http::Message>(std::move(message));
        m_prevMessageReceived = messageToSave.get();
        m_receivedMessageQueue.push(std::move(messageToSave));
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
// TEST_F(BaseStreamProtocolConnection, taking_socket_after_message_availability)

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
