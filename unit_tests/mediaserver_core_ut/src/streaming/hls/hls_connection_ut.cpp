#include <gtest/gtest.h>

#include <streaming/hls/hls_server.h>
#include <network/tcp_listener.h>
#include <common/common_module.h>
#include <test_support/network/tcp_listener_stub.h>

namespace nx_hls {
namespace test {

class QnHttpLiveStreamingProcessorHttpResponse:
    public ::testing::Test
{
public:
    QnHttpLiveStreamingProcessorHttpResponse(nx_http::MimeProtoVersion httpVersion):
        m_commonModule(/*clientMode*/ false),
        m_tcpListener(&m_commonModule),
        m_hlsRequestProcessor(QSharedPointer<AbstractStreamSocket>(), &m_tcpListener)
    {
        m_request.requestLine.version = httpVersion;
    }

    ~QnHttpLiveStreamingProcessorHttpResponse()
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
        m_response.statusLine.statusCode = nx_http::StatusCode::ok;
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
        ASSERT_EQ(nx_http::getHeaderValue(m_response.headers, "Transfer-Encoding"), "chunked");
    }

    void expectEndOfFileSignalledByConnectionClosure()
    {
        const auto it = m_response.headers.find("Connection");
        if (it != m_response.headers.end())
        {
            ASSERT_EQ("close", it->second);
            return;
        }

        if (m_response.statusLine.version == nx_http::http_1_0)
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
    QnCommonModule m_commonModule;
    TcpListenerStub m_tcpListener;
    QnHttpLiveStreamingProcessor m_hlsRequestProcessor;
    nx_http::Request m_request;
    nx_http::Response m_response;
};

//-------------------------------------------------------------------------------------------------
// QnHttpLiveStreamingProcessorHttp11Response

class QnHttpLiveStreamingProcessorHttp11Response:
    public QnHttpLiveStreamingProcessorHttpResponse
{
public:
    QnHttpLiveStreamingProcessorHttp11Response():
        QnHttpLiveStreamingProcessorHttpResponse(nx_http::http_1_1)
    {
    }
};

TEST_F(QnHttpLiveStreamingProcessorHttp11Response, fixed_length_resource)
{
    givenHlsResourceWithFixedLength();
    havingPreparedResponse();

    expectNonZeroContentLengthInResponse();
    expectNoTransferEncodingInResponse();
}

TEST_F(QnHttpLiveStreamingProcessorHttp11Response, unknown_length_resource)
{
    givenHlsResourceWithUnknownLength();
    havingPreparedResponse();

    expectNoContentLengthInResponse();
    expectChunkedTransferEncodingInResponse();
}

//-------------------------------------------------------------------------------------------------
// QnHttpLiveStreamingProcessorHttp10Response

class QnHttpLiveStreamingProcessorHttp10Response:
    public QnHttpLiveStreamingProcessorHttpResponse
{
public:
    QnHttpLiveStreamingProcessorHttp10Response():
        QnHttpLiveStreamingProcessorHttpResponse(nx_http::http_1_0)
    {
    }
};

TEST_F(QnHttpLiveStreamingProcessorHttp10Response, fixed_length_resource)
{
    givenHlsResourceWithFixedLength();
    havingPreparedResponse();

    expectNonZeroContentLengthInResponse();
    expectNoTransferEncodingInResponse();
}

TEST_F(QnHttpLiveStreamingProcessorHttp10Response, unknown_length_resource)
{
    givenHlsResourceWithUnknownLength();
    havingPreparedResponse();

    expectNoContentLengthInResponse();
    expectNoTransferEncodingInResponse();
    expectEndOfFileSignalledByConnectionClosure();
}

TEST(HLSMimeTypes, main)
{
    QnCommonModule commonModule(false);
    TcpListenerStub tcpListener(&commonModule);

    class HlsServerTest : public nx_hls::QnHttpLiveStreamingProcessor
    {
    public:
        using nx_hls::QnHttpLiveStreamingProcessor::QnHttpLiveStreamingProcessor;
        const char *mimeTypeByExtension(const QString& extension) const
        {
            return nx_hls::QnHttpLiveStreamingProcessor::mimeTypeByExtension(extension);
        }
    } hlsServerTest(QSharedPointer<AbstractStreamSocket>(), &tcpListener);

    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("m3u8")), lit("application/vnd.apple.mpegurl"));
    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("M3U8")), lit("application/vnd.apple.mpegurl"));

    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("m3u")), lit("audio/mpegurl"));
    ASSERT_EQ(QString::fromLocal8Bit(hlsServerTest.mimeTypeByExtension("M3U")), lit("audio/mpegurl"));
}

} // namespace test
} // namespace nx_hls
