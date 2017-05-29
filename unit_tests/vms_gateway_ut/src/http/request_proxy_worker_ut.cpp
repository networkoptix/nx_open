#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>

#include <http/request_proxy_worker.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

static const char* const kStaticResourcePath = "/RequestProxyWorker/static";

class RequestProxyWorker:
    public ::testing::Test,
    public AbstractResponseSender
{
protected:
    void whenStreamingMessageBody()
    {
        initializeProxyWorker(kStaticResourcePath);
    }

    void thenMessageBodyEndIsReported()
    {
        m_messageBodyEndReported.get_future().wait();
    }

private:
    std::unique_ptr<gateway::RequestProxyWorker> m_requestProxyWorker;
    TestHttpServer m_httpServer;
    nx::Buffer m_staticResource;
    nx::utils::promise<void> m_messageBodyEndReported;
    std::unique_ptr<nx_http::AbstractMsgBodySource> m_messageBodySource;

    virtual void SetUp() override
    {
        m_staticResource.resize(16*1024);
        std::generate(m_staticResource.begin(), m_staticResource.end(), &rand);

        m_httpServer.registerStaticProcessor(
            kStaticResourcePath,
            m_staticResource,
            "video/mp2t");

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    virtual void sendResponse(
        nx_http::RequestResult requestResult,
        boost::optional<nx_http::Response> /*response*/) override
    {
        if (requestResult.dataSource)
        {
            m_messageBodySource = std::move(requestResult.dataSource);
            startReadingMessageBody();
        }
    }

    void initializeProxyWorker(const nx::String& path)
    {
        nx_http::Request translatedRequest;
        translatedRequest.requestLine.method = nx_http::Method::GET;
        translatedRequest.requestLine.url = path;
        translatedRequest.requestLine.version = nx_http::http_1_1;

        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(m_httpServer.serverAddress()))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));

        m_requestProxyWorker = std::make_unique<gateway::RequestProxyWorker>(
            "not_used_in_streaming_mode",
            std::move(translatedRequest),
            this,
            std::move(tcpSocket));
    }

    void startReadingMessageBody()
    {
        using namespace std::placeholders;

        m_messageBodySource->readAsync(
            std::bind(&RequestProxyWorker::onSomeMessageBodyRead, this, _1, _2));
    }

    void onSomeMessageBodyRead(SystemError::ErrorCode systemErrorCode, nx::Buffer buffer)
    {
        ASSERT_EQ(SystemError::noError, systemErrorCode);

        if (buffer.isEmpty())
        {
            m_messageBodySource.reset();
            m_messageBodyEndReported.set_value();
            return;
        }

        startReadingMessageBody();
    }
};

TEST_F(RequestProxyWorker, streaming_message_body_end_is_reported)
{
    whenStreamingMessageBody();
    thenMessageBodyEndIsReported();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
