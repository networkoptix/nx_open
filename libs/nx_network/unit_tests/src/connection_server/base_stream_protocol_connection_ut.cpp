// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::server::test {

namespace {

class DummyOutput:
    public utils::bstream::AbstractOutput
{
public:
    virtual int write(const void* /*data*/, size_t count) override
    {
        return (int)count;
    }
};

class AsyncChannelToStreamSocketAdapter:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    AsyncChannelToStreamSocketAdapter(aio::AbstractAsyncChannel* asyncChannel):
        base_type(&m_tcpSocket),
        m_asyncChannel(asyncChannel)
    {
        m_tcpSocket.setNonBlockingMode(true);
        m_asyncChannel->bindToAioThread(getAioThread());
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_asyncChannel->bindToAioThread(aioThread);
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override
    {
        m_asyncChannel->readSomeAsync(buffer, std::move(handler));
    }

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override
    {
        m_asyncChannel->sendAsync(buffer, std::move(handler));
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
            "Connection: close\r\n"
            "Content-Type: video/mp2t\r\n"
            "Server: Notwerk Iptox\r\n" //< Avoid "check for open source failed" errors in CI.
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
            "Connection: close\r\n"
            "Content-Length: 25\r\n"
            "Content-Type: video/mp2t\r\n"
            "Server: Notwerk Iptox\r\n" //< Avoid "check for open source failed" errors in CI.
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
            "Connection: close\r\n"
            "Content-Type: video/mp2t\r\n"
            "Server: Notwerk Iptox\r\n" //< Avoid "check for open source failed" errors in CI.
            "\r\n";
    }
};
static const HttpMessageWithoutBody kHttpMessageWithoutBody;

//-------------------------------------------------------------------------------------------------

using TestHttpConnection = nx::network::http::AsyncMessagePipeline;

} // namespace

/**
 * Testing BaseStreamProtocolConnection using AsyncMessagePipeline since
 *   Http already has convenient infrastructure around it.
 */
class BaseStreamProtocolConnection:
    public ::testing::Test
{
public:
    BaseStreamProtocolConnection():
        m_asyncChannel(
            &m_input,
            &m_dummyOutput,
            aio::test::AsyncChannel::InputDepletionPolicy::retry)
    {
        m_connection = std::make_unique<TestHttpConnection>(
            std::make_unique<AsyncChannelToStreamSocketAdapter>(&m_asyncChannel));
        m_connection->setMessageHandler(
            [this](auto&&... args) { saveMessage(std::move(args)...); });
        m_connection->setOnSomeMessageBodyAvailable(
            [this](auto&&... args) { saveSomeBody(std::move(args)...); });
        m_connection->registerCloseHandler(
            [this](auto&&... args) { saveConnectionClosedEvent(std::move(args)...); });
        m_connection->startReadingConnection();
    }

    ~BaseStreamProtocolConnection()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
        m_asyncChannel.pleaseStopSync();
    }

protected:
    void givenBrokenConnection()
    {
        m_asyncChannel.setSendErrorState(SystemError::connectionReset);
    }

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

    void whenCloseConnection()
    {
        m_connection->closeConnection(SystemError::connectionAbort);
    }

    template<typename Handler>
    void whenSendMessage(Handler handler)
    {
        m_connection->sendMessage(
            http::Message(http::MessageType::request),
            [this, handler = std::move(handler)](
                SystemError::ErrorCode sysErrorCode)
            {
                handler();
                m_sendResults.push(sysErrorCode);
            });
    }

    void whenSendMessage()
    {
        whenSendMessage([]() {});
    }

    void thenMessageIsReported()
    {
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
                    NX_MUTEX_LOCKER lock(&m_mutex);
                    if (message.fullMessage == messageReceived->toString())
                        break;
                }

                std::this_thread::yield();
            }
        }
    }

    void thenMessageHasBeenSent()
    {
        ASSERT_EQ(SystemError::noError, m_sendResults.pop());
    }

    void thenMessageSendFailed()
    {
        ASSERT_NE(SystemError::noError, m_sendResults.pop());
    }

    void andConnectionClosureIsReported()
    {
        ASSERT_NE(SystemError::noError, m_connectionClosedEvents.pop().first);
    }

    virtual void saveMessage(nx::network::http::Message message)
    {
        auto messageToSave = std::make_unique<nx::network::http::Message>(std::move(message));
        m_prevMessageReceived = messageToSave.get();
        m_receivedMessageQueue.push(std::move(messageToSave));
    }

    void freeConnection()
    {
        m_asyncChannel.cancelIOSync(aio::etNone);
        m_connection.reset();
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
    nx::utils::bstream::Pipe m_input;
    aio::test::AsyncChannel m_asyncChannel;
    std::unique_ptr<TestHttpConnection> m_connection;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Message>> m_receivedMessageQueue;
    nx::utils::bstream::test::NotifyingOutput m_receivedMsgBody;
    nx::Buffer m_expectedBody;
    std::vector<HttpMessageTestData> m_messagesSent;
    nx::network::http::Message* m_prevMessageReceived = nullptr;
    nx::Mutex m_mutex;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_sendResults;
    nx::utils::SyncMultiQueue<SystemError::ErrorCode, bool> m_connectionClosedEvents;
    DummyOutput m_dummyOutput;

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
            for (std::size_t pos = 0; pos < message.fullMessage.size(); pos += step)
            {
                const auto bytesToRead =
                    std::min<std::size_t>(step, message.fullMessage.size() - pos);
                m_input.write(message.fullMessage.data() + pos, bytesToRead);
            }
        }
    }

    void saveSomeBody(nx::Buffer bodyBuffer)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        m_prevMessageReceived->response->messageBody.append(bodyBuffer);
        m_receivedMsgBody.write(bodyBuffer.data(), bodyBuffer.size());
    }

    void saveConnectionClosedEvent(
        SystemError::ErrorCode systemErrorCode,
        bool connectionDestroyed)
    {
        m_connectionClosedEvents.push(systemErrorCode, connectionDestroyed);
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

TEST_F(BaseStreamProtocolConnection, can_be_removed_in_send_completion_handler)
{
    whenSendMessage([this]() { freeConnection(); });

    thenMessageHasBeenSent();
    // andProcessHasNotCrashed()
}

TEST_F(BaseStreamProtocolConnection, message_send_error_is_delivered)
{
    givenBrokenConnection();

    whenSendMessage();

    thenMessageSendFailed();
    andConnectionClosureIsReported();
}

TEST_F(BaseStreamProtocolConnection, cancel_send_on_send_failure_is_allowed)
{
    givenBrokenConnection();

    whenSendMessage([this]() { connection().cancelWrite(); } );

    thenMessageSendFailed();
    // andProcessHasNotCrashed()
}

TEST_F(BaseStreamProtocolConnection, cancel_send_on_send_success_is_allowed)
{
    whenSendMessage([this]() { connection().cancelWrite(); });

    thenMessageHasBeenSent();
    // andProcessHasNotCrashed()
}

TEST_F(BaseStreamProtocolConnection, send_via_closed_connection_is_a_failure)
{
    whenCloseConnection();
    whenSendMessage([]() {});

    thenMessageSendFailed();
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
        m_readBuffer.reserve(expectedBody().size());

        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            [this](auto&&... args) { onSomeBytesRead(std::move(args)...); });

        m_done.get_future().wait();
    }

    virtual void saveMessage(nx::network::http::Message message)
    {
        base_type::saveMessage(message);

        m_streamSocket = connection().takeSocket();
    }

private:
    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_streamSocket;
    nx::Buffer m_readBuffer;
    nx::utils::promise<void> m_done;

    void onSomeBytesRead(
        SystemError::ErrorCode sysErrorCode, std::size_t /*bytesRead*/)
    {
        ASSERT_EQ(SystemError::noError, sysErrorCode);

        if (m_readBuffer == expectedBody())
        {
            m_done.set_value();
            return;
        }

        ASSERT_TRUE(expectedBody().starts_with(m_readBuffer));

        m_streamSocket->readSomeAsync(
            &m_readBuffer,
            [this](auto&&... args) { onSomeBytesRead(std::move(args)...); });
    }
};

TEST_F(BaseStreamProtocolConnectionTakeSocket, taking_socket_does_not_lose_data)
{
    whenReadMessageWithIncompleteBody();
    whenTakenSocketInProcessMessageHandler();
    thenMessageBodyCanBeReadFromSocket();
}

} // namespace nx::network::server::test
