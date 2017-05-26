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

static const char kHttpRequestWithIncompleteInfiniteBody[] = R"http(
HTTP/1.1 200 OK
Server: Network Optix
Content-Type: video/mp2t
Connection: close

Here goes mpeg2-ts video.
)http";

static const char kHttpRequestBody[] = "Here goes mpeg2-ts video.";

static const std::size_t kHttpRequestLen = sizeof(kHttpRequestWithIncompleteInfiniteBody) - 1;

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
        m_input.write(kHttpRequestWithIncompleteInfiniteBody, kHttpRequestLen);
    }

    void thenMessageIsReported()
    {
        m_receivedMessageQueue.pop();
    }
    
    void thenAvailableMessageBodyIsReported()
    {
        m_receivedMsgBody.waitForReceivedDataToMatch(kHttpRequestBody);
    }

private:
    nx::utils::bstream::ReflectingPipeline m_input;
    aio::test::AsyncChannel m_asyncChannel;
    std::unique_ptr<TestHttpConnection> m_connection;
    nx::utils::SyncQueue<nx_http::Message> m_receivedMessageQueue;
    nx::utils::bstream::test::NotifyingOutput m_receivedMsgBody;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        TestHttpConnection* connection) override
    {
        ASSERT_EQ(m_connection.get(), connection);
    }

    void saveMessage(nx_http::Message message)
    {
        m_receivedMessageQueue.push(std::move(message));
    }

    void saveSomeBody(nx::Buffer bodyBuffer)
    {
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
    thenAvailableMessageBodyIsReported();
}

//TEST_F(BaseStreamProtocolConnection, message_body_is_reported_on_availability)

//TEST_F(BaseStreamProtocolConnection, message_without_message_body)

//TEST_F(BaseStreamProtocolConnection, receiving_multiple_messages_with_body_over_same_connection)

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
