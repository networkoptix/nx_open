#include <gtest/gtest.h>

#include <nx/vms/server/hls/hls_server.h>
#include <network/tcp_listener.h>
#include <nx/core/access/access_types.h>
#include <common/common_module.h>
#include <test_support/network/tcp_listener_stub.h>
#include <media_server/media_server_module.h>

namespace nx {
namespace vms::server {
namespace hls {
namespace test {

class HttpLiveStreamingProcessorHttpResponse:
    public ::testing::Test
{
public:
    HttpLiveStreamingProcessorHttpResponse(nx::network::http::MimeProtoVersion httpVersion):
        m_tcpListener(m_serverModule.commonModule()),
        m_hlsRequestProcessor(&m_serverModule, std::unique_ptr<nx::network::AbstractStreamSocket>(), &m_tcpListener)
    {
        m_request.requestLine.version = httpVersion;
    }

    ~HttpLiveStreamingProcessorHttpResponse()
    {
    }

    void givenHlsResourceWithFixedLength()
    {
        m_response.headers.emplace("Content-Length", "123456");
    }

    void givenHlsResourceWithUnknownLength()
    {
    }

    void havingPreparedResponse()
    {
        m_response.statusLine.version = m_request.requestLine.version;
        m_response.statusLine.statusCode = nx::network::http::StatusCode::ok;
        m_hlsRequestProcessor.prepareResponse(m_request, &m_response);
    }

    void expectNonZeroContentLengthInResponse()
    {
        const auto it = m_response.headers.find("Content-Length");
        ASSERT_NE(m_response.headers.end(), it);
        ASSERT_GT(it->second.toInt(), 0);
    }

    void expectNoTransferEncodingInResponse()
    {
        ASSERT_TRUE(m_response.headers.find("Transfer-Encoding") == m_response.headers.end());
    }

    void expectNoContentLengthInResponse()
    {
        ASSERT_TRUE(m_response.headers.find("Content-Length") == m_response.headers.end());
    }

    void expectChunkedTransferEncodingInResponse()
    {
        ASSERT_EQ(nx::network::http::getHeaderValue(m_response.headers, "Transfer-Encoding"), "chunked");
    }

    void expectEndOfFileSignalledByConnectionClosure()
    {
        const auto it = m_response.headers.find("Connection");
        if (it != m_response.headers.end())
        {
            ASSERT_EQ("close", it->second);
            return;
        }

        if (m_response.statusLine.version == nx::network::http::http_1_0)
        {
            // In HTTP/1.0 connection is NOT persistent by default.
            return;
        }
        else
        {
            // In HTTP/1.1 connection is persistent by default.
            ASSERT_TRUE(false);
        }
    }

private:
    QnMediaServerModule m_serverModule;
    TcpListenerStub m_tcpListener;
    HttpLiveStreamingProcessor m_hlsRequestProcessor;
    nx::network::http::Request m_request;
    nx::network::http::Response m_response;
};

//-------------------------------------------------------------------------------------------------
// HttpLiveStreamingProcessorHttp11Response

class HttpLiveStreamingProcessorHttp11Response:
    public HttpLiveStreamingProcessorHttpResponse
{
public:
    HttpLiveStreamingProcessorHttp11Response():
        HttpLiveStreamingProcessorHttpResponse(nx::network::http::http_1_1)
    {
    }
};

TEST_F(HttpLiveStreamingProcessorHttp11Response, fixed_length_resource)
{
    givenHlsResourceWithFixedLength();
    havingPreparedResponse();

    expectNonZeroContentLengthInResponse();
    expectNoTransferEncodingInResponse();
}

TEST_F(HttpLiveStreamingProcessorHttp11Response, unknown_length_resource)
{
    givenHlsResourceWithUnknownLength();
    havingPreparedResponse();

    expectNoContentLengthInResponse();
    expectChunkedTransferEncodingInResponse();
}

//-------------------------------------------------------------------------------------------------
// HttpLiveStreamingProcessorHttp10Response

class HttpLiveStreamingProcessorHttp10Response:
    public HttpLiveStreamingProcessorHttpResponse
{
public:
    HttpLiveStreamingProcessorHttp10Response():
        HttpLiveStreamingProcessorHttpResponse(nx::network::http::http_1_0)
    {
    }
};

TEST_F(HttpLiveStreamingProcessorHttp10Response, fixed_length_resource)
{
    givenHlsResourceWithFixedLength();
    havingPreparedResponse();

    expectNonZeroContentLengthInResponse();
    expectNoTransferEncodingInResponse();
}

TEST_F(HttpLiveStreamingProcessorHttp10Response, unknown_length_resource)
{
    givenHlsResourceWithUnknownLength();
    havingPreparedResponse();

    expectNoContentLengthInResponse();
    expectNoTransferEncodingInResponse();
    expectEndOfFileSignalledByConnectionClosure();
}

TEST(HLSMimeTypes, main)
{
    QnMediaServerModule mediaServerModule;
    TcpListenerStub tcpListener(mediaServerModule.commonModule());

    class HlsServerTest : public nx::vms::server::hls::HttpLiveStreamingProcessor
    {
    public:
        using nx::vms::server::hls::HttpLiveStreamingProcessor::HttpLiveStreamingProcessor;
        const char *mimeTypeByExtension(const QString& extension) const
        {
            return nx::vms::server::hls::HttpLiveStreamingProcessor::mimeTypeByExtension(extension);
        }
    } hlsServerTest(&mediaServerModule, std::unique_ptr<nx::network::AbstractStreamSocket>(), &tcpListener);

    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("m3u8")), lit("application/vnd.apple.mpegurl"));
    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("M3U8")), lit("application/vnd.apple.mpegurl"));

    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("m3u")), lit("audio/mpegurl"));
    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("M3U")), lit("audio/mpegurl"));
}

} // namespace test
} // namespace hls
} // namespace vms::server
} // namespace nx
