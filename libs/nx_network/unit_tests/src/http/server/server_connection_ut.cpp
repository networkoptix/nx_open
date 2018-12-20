#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/network/aio/timer.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace http {

namespace {

class HttpAsyncServerConnectionTest:
    public ::testing::Test
{
public:
    HttpAsyncServerConnectionTest():
        m_testHttpServer(std::make_unique<TestHttpServer>())
    {
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};

class DelayingRequestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    static const QString PATH;

    virtual void processRequest(
        http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_timer.start(
            std::chrono::seconds(5),
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(nx::network::http::StatusCode::ok);
            });
    }

private:
    nx::network::aio::Timer m_timer;
};

const QString DelayingRequestHandler::PATH = "/tst";

} // namespace

TEST_F(HttpAsyncServerConnectionTest, connectionRemovedBeforeRequestHasBeenProcessed)
{
    m_testHttpServer->registerRequestProcessor<DelayingRequestHandler>(
        DelayingRequestHandler::PATH,
        [&]()->std::unique_ptr<DelayingRequestHandler> {
            return std::make_unique<DelayingRequestHandler>();
        });

    ASSERT_TRUE(m_testHttpServer->bindAndListen());

    //fetching mjpeg with async http client
    const nx::utils::Url url(QString("http://127.0.0.1:%1%2").
        arg(m_testHttpServer->serverAddress().port).arg(DelayingRequestHandler::PATH));

    auto client = nx::network::http::AsyncHttpClient::create();
    client->doGet(url);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    client.reset();

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(HttpAsyncServerConnectionTest, multipleRequestsTest)
{
    static const char testData[] =
        "GET /cdb/event/subscribe HTTP/1.1\r\n"
        "Date: Fri, 20 May 2016 05:43:16 -0700\r\n"
        "Host: cloud-demo.hdw.mx\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: Nx Witness/3.0.0.12042 (Network Optix) Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:36.0)\r\n"
        "Authorization: Digest nonce=\"15511187291719590911\", realm=\"VMS\", "
        "response=\"68cc3d8c30c7bff77a623b36e7210bb3\", uri=\"/cdb/event/subscribe\", username=\"df6a3827-56c7-4ff8-b38e-67993983d5d8\"\r\n"
        "X-Nx-User-Name: df6a3827-56c7-4ff8-b38e-67993983d5d8\r\n"
        "Accept-Encoding: gzip\r\n"
        "\r\n"
        "GET /cdb/event/subscribe HTTP/1.1\r\n"
        "Date: Fri, 20 May 2016 05:43:19 -0700\r\n"
        "Host: cloud-demo.hdw.mx\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: Nx Witness/3.0.0.12042 (Network Optix) Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:36.0)\r\n"
        "Authorization: Digest nonce=\"15511187291719590911\", realm=\"VMS\", "
        "response=\"68cc3d8c30c7bff77a623b36e72190bb3\", uri=\"/cdb/event/subscribe\", username=\"df6a3827-56c7-4ff8-b38e-67993983d5d8\"\r\n"
        "X-Nx-User-Name: df6a3827-56c7-4ff8-b38e-67993983d5d8\r\n"
        "Accept-Encoding: gzip\r\n"
        "\r\n"
        "GET /cdb/event/subscribe HTTP/1.1\r\n"
        "Date: Fri, 20 May 2016 05:43:22 -0700\r\n"
        "Host: cloud-demo.hdw.mx\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: Nx Witness/3.0.0.12042 (Network Optix) Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:36.0)\r\n"
        "Authorization: Digest nonce=\"15511187291719590911\", realm=\"VMS\", "
        "response=\"68cc3d8c30c7bff77a623b36e7210bb3\", uri=\"/cdb/event/subscribe\", username=\"df6a3827-56c7-4ff8-b38e-67993983d5d8\"\r\n"
        "X-Nx-User-Name: df6a3827-56c7-4ff8-b38e-67993983d5d8\r\n"
        "Accept-Encoding: gzip\r\n";

    ASSERT_TRUE(m_testHttpServer->bindAndListen());

    const auto socket = std::make_unique<nx::network::TCPSocket>(
        SocketFactory::tcpServerIpVersion());

    ASSERT_TRUE(socket->connect(m_testHttpServer->serverAddress(), nx::network::kNoTimeout));
    ASSERT_EQ((int)sizeof(testData) - 1, socket->send(testData, sizeof(testData) - 1));
}

//-------------------------------------------------------------------------------------------------

namespace {

class PipeliningTestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    static const nx::String PATH;

    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        requestContext.response->headers.emplace(
            "Seq",
            http::getHeaderValue(requestContext.request.headers, "Seq"));

        completionHandler(http::RequestResult(
            http::StatusCode::ok,
            std::make_unique<http::BufferSource>("text/plain", "bla-bla-bla")));
    }
};

const nx::String PipeliningTestHandler::PATH = "/tst";

} // namespace

class HttpAsyncServerConnectionRequestPipelining:
    public HttpAsyncServerConnectionTest
{
    using base_type = HttpAsyncServerConnectionTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_testHttpServer->registerRequestProcessor<PipeliningTestHandler>(
            PipeliningTestHandler::PATH,
            [&]()->std::unique_ptr<PipeliningTestHandler>
            {
                return std::make_unique<PipeliningTestHandler>();
            });

        ASSERT_TRUE(m_testHttpServer->bindAndListen());
    }

    void whenSendMultipleRequests(int count)
    {
        //opening connection and sending multiple requests
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::get;
        request.requestLine.version = nx::network::http::http_1_1;
        request.requestLine.url = PipeliningTestHandler::PATH;

        m_socket = SocketFactory::createStreamSocket();
        ASSERT_TRUE(m_socket->connect(SocketAddress(
            HostAddress::localhost,
            m_testHttpServer->serverAddress().port),
            nx::network::kNoTimeout));

        int msgCounter = 0;
        for (; msgCounter < count; ++msgCounter)
        {
            nx::network::http::insertOrReplaceHeader(
                &request.headers,
                nx::network::http::HttpHeader("Seq", nx::String::number(msgCounter)));
            //sending request
            auto serializedMessage = request.serialized();
            ASSERT_EQ(m_socket->send(serializedMessage), serializedMessage.size());
        }
    }

    void thenMultipleResponsesAreReceived(int count)
    {
        int msgCounter = count;
        while (msgCounter > 0)
        {
            if (m_dataSize == 0)
                readMoreData();

            parseDataRead();

            if (m_httpMsgReader.state() == nx::network::http::HttpStreamReader::messageDone)
            {
                ASSERT_TRUE(m_httpMsgReader.message().response != nullptr);
                const auto& response = *m_httpMsgReader.message().response;

                auto seqIter = response.headers.find("Seq");
                ASSERT_TRUE(seqIter != response.headers.end());
                ASSERT_EQ(seqIter->second.toInt(), count - msgCounter);
                --msgCounter;
            }
        }

        ASSERT_EQ(0, msgCounter);
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    nx::network::http::HttpStreamReader m_httpMsgReader;
    nx::Buffer m_readBuf;
    int m_dataSize = 0;

    void readMoreData()
    {
        if (m_readBuf.size() < 64 * 1024)
            m_readBuf.resize(64 * 1024);

        const auto bytesRead = m_socket->recv(
            m_readBuf.data() + m_dataSize,
            m_readBuf.size() - m_dataSize);
        ASSERT_TRUE(bytesRead > 0)
            << SystemError::getLastOSErrorText().toStdString();
        m_dataSize += bytesRead;
    }

    void parseDataRead()
    {
        size_t bytesParsed = 0;
        ASSERT_TRUE(
            m_httpMsgReader.parseBytes(
                QnByteArrayConstRef(m_readBuf, 0, m_dataSize),
                &bytesParsed));
        m_readBuf.remove(0, bytesParsed);
        m_dataSize -= bytesParsed;
    }
};

TEST_F(
    HttpAsyncServerConnectionRequestPipelining,
    responses_are_delivered_in_the_correct_order)
{
    static constexpr int kRequestToSendCount = 17;

    whenSendMultipleRequests(kRequestToSendCount);
    thenMultipleResponsesAreReceived(kRequestToSendCount);
}

//-------------------------------------------------------------------------------------------------
// Connection upgrade tests.

namespace {

class HttpAsyncServerConnectionUpgrade:
    public HttpAsyncServerConnectionTest
{
protected:
    static const QString kTestPath;
    static const nx::network::http::StringType kProtocolToUpgradeTo;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_testHttpServer->registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpAsyncServerConnectionUpgrade::processRequest, this,
                _1, _2));

        ASSERT_TRUE(m_testHttpServer->bindAndListen());
    }

    virtual void TearDown() override
    {
    }

    void issueUpgradeRequest()
    {
        nx::utils::promise<nx::network::http::Response> response;

        nx::network::http::AsyncClient httpClient;
        httpClient.doUpgrade(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setHost("127.0.0.1")
                .setPort(m_testHttpServer->serverAddress().port)
                .setPath(kTestPath),
            kProtocolToUpgradeTo,
            [&response, &httpClient]()
            {
                response.set_value(*httpClient.response());
            });
        m_httpResponse = response.get_future().get();
        httpClient.pleaseStopSync();
    }

    void assertThatResponseIsValid()
    {
        ASSERT_EQ(
            nx::network::http::StatusCode::switchingProtocols,
            m_httpResponse.statusLine.statusCode);
        ASSERT_EQ(
            "Upgrade",
            nx::network::http::getHeaderValue(m_httpResponse.headers, "Connection"));
        ASSERT_EQ(
            kProtocolToUpgradeTo,
            nx::network::http::getHeaderValue(m_httpResponse.headers, "Upgrade"));
    }

    void assertNoMessageBodyHeadersPresent()
    {
        ASSERT_TRUE(m_httpResponse.headers.find("Content-Length") == m_httpResponse.headers.end());
        ASSERT_TRUE(m_httpResponse.headers.find("Content-Type") == m_httpResponse.headers.end());
    }

private:
    nx::network::http::Response m_httpResponse;

    void processRequest(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        completionHandler(nx::network::http::StatusCode::switchingProtocols);
    }
};

const QString HttpAsyncServerConnectionUpgrade::kTestPath("/HttpAsyncServerConnectionUpgrade");
const nx::network::http::StringType HttpAsyncServerConnectionUpgrade::kProtocolToUpgradeTo("TEST/1.0");

} // namespace

TEST_F(HttpAsyncServerConnectionUpgrade, required_headers_are_added_to_response)
{
    issueUpgradeRequest();
    assertThatResponseIsValid();
}

TEST_F(HttpAsyncServerConnectionUpgrade, response_with_1xx_status_does_not_contain_content_length)
{
    issueUpgradeRequest();
    assertNoMessageBodyHeadersPresent();
}

} // namespace nx
} // namespace network
} // namespace http
