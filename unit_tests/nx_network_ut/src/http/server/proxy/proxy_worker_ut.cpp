#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/proxy/proxy_worker.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>

namespace nx_http {
namespace server {
namespace proxy {
namespace test {

static const char* const kStaticResourcePath = "/ProxyWorker/static";
static const char* const kEmptyResourcePath = "/ProxyWorker/noContent";

class ProxyWorker:
    public ::testing::Test,
    public AbstractResponseSender
{
public:
    ~ProxyWorker()
    {
        if (m_requestProxyWorker)
            m_requestProxyWorker->pleaseStopSync();
        if (m_messageBodySource)
            m_messageBodySource->pleaseStopSync();
    }

protected:
    void whenStreamingMessageBody()
    {
        initializeProxyWorker(kStaticResourcePath);
    }

    void whenProxyingRequestToEmptyResource()
    {
        initializeProxyWorker(kEmptyResourcePath);
    }

    void thenMessageBodyEndIsReported()
    {
        m_messageBodyEndReported.get_future().wait();
    }

    void thenEmptyResponseHasBeenDelivered()
    {
        while (!m_proxiedResponse)
            std::this_thread::yield();

        ASSERT_EQ(nx_http::StatusCode::noContent, m_proxiedResponse->statusLine.statusCode);
        ASSERT_TRUE(
            m_proxiedResponse->headers.find("Content-Type") ==
            m_proxiedResponse->headers.end());
        ASSERT_EQ(nullptr, m_messageBodySource);
    }

private:
    std::unique_ptr<proxy::ProxyWorker> m_requestProxyWorker;
    TestHttpServer m_httpServer;
    nx::Buffer m_staticResource;
    nx::utils::promise<void> m_messageBodyEndReported;
    std::unique_ptr<nx_http::AbstractMsgBodySource> m_messageBodySource;
    boost::optional<nx_http::Response> m_proxiedResponse;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_staticResource.resize(16*1024);
        std::generate(m_staticResource.begin(), m_staticResource.end(), &rand);

        m_httpServer.registerStaticProcessor(
            kStaticResourcePath,
            m_staticResource,
            "video/mp2t");

        m_httpServer.registerRequestProcessorFunc(
            kEmptyResourcePath,
            std::bind(&ProxyWorker::returnEmptyHttpResponse, this, _1, _2, _3, _4, _5));

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    virtual void sendResponse(
        nx_http::RequestResult requestResult,
        boost::optional<nx_http::Response> response) override
    {
        m_proxiedResponse = response;

        if (requestResult.dataSource)
        {
            m_messageBodySource = std::move(requestResult.dataSource);
            startReadingMessageBody();
        }
    }

    void initializeProxyWorker(const nx::String& path)
    {
        nx_http::Request translatedRequest;
        translatedRequest.requestLine.method = nx_http::Method::get;
        translatedRequest.requestLine.url = path;
        translatedRequest.requestLine.version = nx_http::http_1_1;

        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(m_httpServer.serverAddress()))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));

        m_requestProxyWorker = std::make_unique<proxy::ProxyWorker>(
            "not_used_in_streaming_mode",
            std::move(translatedRequest),
            this,
            std::move(tcpSocket));
    }

    void startReadingMessageBody()
    {
        using namespace std::placeholders;

        m_messageBodySource->readAsync(
            std::bind(&ProxyWorker::onSomeMessageBodyRead, this, _1, _2));
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

    void returnEmptyHttpResponse(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        completionHandler(nx_http::StatusCode::noContent);
    }
};

TEST_F(ProxyWorker, streaming_message_body_end_is_reported)
{
    whenStreamingMessageBody();
    thenMessageBodyEndIsReported();
}

TEST_F(ProxyWorker, proxying_request_without_message_body)
{
    whenProxyingRequestToEmptyResource();
    thenEmptyResponseHasBeenDelivered();
}

} // namespace test
} // namespace proxy
} // namespace server
} // namespace nx_http
