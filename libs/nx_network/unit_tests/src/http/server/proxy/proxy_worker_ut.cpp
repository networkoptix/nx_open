// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/proxy/proxy_worker.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>

namespace nx::network::http::server::proxy::test {

static const char* const kStaticResourcePath = "/ProxyWorker/static";
static const char* const kEmptyResourcePath = "/ProxyWorker/noContent";

class HttpProxyWorker:
    public ::testing::Test
{
public:
    ~HttpProxyWorker()
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
        const auto response = m_proxiedResponse.get_future().get();

        ASSERT_EQ(nx::network::http::StatusCode::noContent, response.statusLine.statusCode);
        ASSERT_FALSE(response.headers.contains("Content-Type"));
        ASSERT_EQ(nullptr, m_messageBodySource);
    }

private:
    std::unique_ptr<proxy::ProxyWorker> m_requestProxyWorker;
    TestHttpServer m_httpServer;
    nx::Buffer m_staticResource;
    nx::utils::promise<void> m_messageBodyEndReported;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> m_messageBodySource;
    std::promise<nx::network::http::Response> m_proxiedResponse;

    virtual void SetUp() override
    {
        m_staticResource.resize(16*1024);
        std::generate(m_staticResource.begin(), m_staticResource.end(), &rand);

        m_httpServer.registerStaticProcessor(
            kStaticResourcePath,
            m_staticResource,
            "video/mp2t");

        m_httpServer.registerRequestProcessorFunc(
            kEmptyResourcePath,
            [this](auto&&... args) { returnEmptyHttpResponse(std::forward<decltype(args)>(args)...); });

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void proxyResponse(nx::network::http::RequestResult requestResult)
    {
        http::Response resp;
        resp.statusLine.statusCode = requestResult.statusCode;
        resp.headers = requestResult.headers;
        m_proxiedResponse.set_value(std::move(resp));

        if (requestResult.body)
        {
            m_messageBodySource = std::move(requestResult.body);
            startReadingMessageBody();
        }
    }

    void initializeProxyWorker(const std::string& path)
    {
        nx::network::http::Request translatedRequest;
        translatedRequest.requestLine.method = nx::network::http::Method::get;
        translatedRequest.requestLine.url = path;
        translatedRequest.requestLine.version = nx::network::http::http_1_1;
        translatedRequest.headers.emplace(
            "Host",
            m_httpServer.serverAddress().toString());

        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(m_httpServer.serverAddress(), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText();
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));

        m_requestProxyWorker = std::make_unique<proxy::ProxyWorker>(
            "not_used_in_streaming_mode",
            http::kUrlSchemeName,
            true,
            std::move(translatedRequest),
            nullptr,
            std::move(tcpSocket));

        m_requestProxyWorker->start(
            [this](auto&&... args) { proxyResponse(std::forward<decltype(args)>(args)...); });
    }

    void startReadingMessageBody()
    {
        m_messageBodySource->readAsync(
            [this](auto&&... args) { onSomeMessageBodyRead(std::forward<decltype(args)>(args)...); });
    }

    void onSomeMessageBodyRead(SystemError::ErrorCode systemErrorCode, nx::Buffer buffer)
    {
        ASSERT_EQ(SystemError::noError, systemErrorCode);

        if (buffer.empty())
        {
            m_messageBodySource.reset();
            m_messageBodyEndReported.set_value();
            return;
        }

        startReadingMessageBody();
    }

    void returnEmptyHttpResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        completionHandler(nx::network::http::StatusCode::noContent);
    }
};

TEST_F(HttpProxyWorker, streaming_message_body_end_is_reported)
{
    whenStreamingMessageBody();
    thenMessageBodyEndIsReported();
}

TEST_F(HttpProxyWorker, proxying_request_without_message_body)
{
    whenProxyingRequestToEmptyResource();
    thenEmptyResponseHasBeenDelivered();
}

} // namespace nx::network::http::server::proxy::test
