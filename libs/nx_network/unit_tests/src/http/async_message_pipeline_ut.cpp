// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/async_message_pipeline.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::test {

class TestBodySource:
    public BufferSource
{
    using base_type = BufferSource;

public:
    using base_type::base_type;

    virtual void readAsync(CompletionHandler completionHandler) override
    {
        if (m_pauseOnRead)
        {
            m_paused.push();
            m_resume.pop();
        }

        base_type::readAsync(std::move(completionHandler));
    }

    void setPauseOnRead(bool val)
    {
        m_pauseOnRead = val;
    }

    void waitTillPaused()
    {
        m_paused.pop();
    }

    void resume()
    {
        m_resume.push();
    }

private:
    bool m_pauseOnRead = false;
    nx::utils::SyncQueue<void> m_paused;
    nx::utils::SyncQueue<void> m_resume;
};

//-------------------------------------------------------------------------------------------------

class SocketMock:
    public TCPSocket
{
    using base_type = TCPSocket;

public:
    using base_type::base_type;

    virtual void sendAsync(
        const nx::Buffer* buf,
        IoCompletionHandler handler) override
    {
        if (m_fail)
        {
            return post(
                [handler = std::move(handler)]()
                {
                    handler(SystemError::connectionReset, 0);
                });
        }

        base_type::sendAsync(buf, std::move(handler));
    }

    void setFailState()
    {
        m_fail = true;
    }

private:
    bool m_fail = false;
};

//-------------------------------------------------------------------------------------------------

static constexpr char kSaveRequestPath[] = "/save-request";

class HttpAsyncMessagePipeline:
    public ::testing::Test
{
public:
    ~HttpAsyncMessagePipeline()
    {
        if (m_msgPipeline)
            m_msgPipeline->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        m_httpServer.registerRequestProcessorFunc(
            kSaveRequestPath,
            [this](auto&&... args) { saveRequest(std::forward<decltype(args)>(args)...); });

        ASSERT_TRUE(m_httpServer.bindAndListen());

        auto connection = std::make_unique<SocketMock>(AF_INET);
        ASSERT_TRUE(connection->connect(m_httpServer.serverAddress(), kNoTimeout));
        ASSERT_TRUE(connection->setNonBlockingMode(true));

        m_msgPipeline = std::make_unique<AsyncMessagePipeline>(std::move(connection));
    }

    void whenSendMessageWithBody()
    {
        sendMessage(16 * 1024);
    }

    void whenSendMessageWithEmptyBody()
    {
        sendMessage(0);
    }

    void whenMessageBodySourcePaused()
    {
        m_body->waitTillPaused();
    }

    void thenSendSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_sendResults.pop());
    }

    void thenSendFailed()
    {
        ASSERT_NE(SystemError::noError, m_sendResults.pop());
    }

    void andFullMessageIsDelivered()
    {
        ASSERT_EQ(m_lastRequestSent, m_requestsReceived.pop());
    }

    void setPauseMessageBodySourceRead(bool pause)
    {
        m_pauseMessageBodySourceRead = pause;
        if (m_body)
            m_body->setPauseOnRead(pause);
    }

    void setConnectionToFailState()
    {
        static_cast<SocketMock*>(m_msgPipeline->socket().get())->setFailState();
    }

    void resumeMessageBodySource()
    {
        m_body->setPauseOnRead(false);
        m_body->resume();
    }

private:
    Request m_lastRequestSent;
    nx::utils::SyncQueue<Request> m_requestsReceived;
    nx::network::http::TestHttpServer m_httpServer;
    std::unique_ptr<AsyncMessagePipeline> m_msgPipeline;
    TestBodySource* m_body = nullptr;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_sendResults;
    bool m_pauseMessageBodySourceRead = false;

    void saveRequest(
        RequestContext requestContext,
        RequestProcessedHandler handler)
    {
        m_requestsReceived.push(std::move(requestContext.request));
        handler(StatusCode::ok);
    }

    void sendMessage(std::size_t bodySize)
    {
        m_lastRequestSent = prepareRequest(bodySize);

        // Sending message body separately from the message.
        auto message = Message(MessageType::request);
        *message.request = m_lastRequestSent;
        std::unique_ptr<TestBodySource> body;

        if (!m_lastRequestSent.messageBody.empty())
        {
            body = std::make_unique<TestBodySource>(
                "text/plain",
                std::exchange(message.request->messageBody, {}));
            m_body = body.get();
            m_body->setPauseOnRead(m_pauseMessageBodySourceRead);
        }

        m_msgPipeline->sendMessage(
            std::move(message),
            std::move(body),
            [this](auto resultCode) { m_sendResults.push(resultCode); });
    }

    Request prepareRequest(std::size_t bodySize)
    {
        Request request;
        request.requestLine.method = Method::post;
        request.requestLine.url = kSaveRequestPath;
        request.requestLine.version = http_1_0;
        if (bodySize > 0)
        {
            request.messageBody = nx::utils::generateRandomName(bodySize);
            request.headers.emplace("Content-Type", "text/plain");
            request.headers.emplace("Content-Length", std::to_string(request.messageBody.size()));
        }
        else
        {
            request.headers.emplace("Content-Length", "0");
        }

        return request;
    }
};

TEST_F(HttpAsyncMessagePipeline, message_with_body_is_delivered)
{
    whenSendMessageWithBody();

    thenSendSucceeded();
    andFullMessageIsDelivered();
}

TEST_F(HttpAsyncMessagePipeline, message_with_empty_body_delivered)
{
    whenSendMessageWithEmptyBody();

    thenSendSucceeded();
    andFullMessageIsDelivered();
}

TEST_F(HttpAsyncMessagePipeline, connection_closure_interrupts_sending_request)
{
    setConnectionToFailState();

    whenSendMessageWithBody();
    thenSendFailed();
}

TEST_F(HttpAsyncMessagePipeline, connection_closure_interrupts_sending_body)
{
    setPauseMessageBodySourceRead(true);

    whenSendMessageWithBody();

    whenMessageBodySourcePaused();
    setConnectionToFailState();
    resumeMessageBodySource();

    thenSendFailed();
}

} // namespace nx::network::http::test
