// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include <QtCore/QFile>

#include <gtest/gtest.h>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/random.h>
#include <nx/utils/std/filesystem.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::test {

namespace {

class TestBodySource:
    public BufferSource
{
    using base_type = BufferSource;

public:
    TestBodySource(
        const std::string& mimeType,
        nx::Buffer msgBody,
        std::optional<uint64_t> contentLengthOverride)
        :
        base_type(mimeType, std::move(msgBody)),
        m_contentLengthOverride(contentLengthOverride)
    {
    }

    virtual std::optional<uint64_t> contentLength() const override
    {
        return m_contentLengthOverride;
    }

private:
    std::optional<uint64_t> m_contentLengthOverride;
};

} // namespace

//-------------------------------------------------------------------------------------------------

static constexpr char kStaticDataPath[] = "/test";
static constexpr char kStaticData[] = "SimpleTest";
static constexpr char kTestFileHttpPath[] = "/test.txt";
static constexpr char kTestFileMimeType[] = "text/plain";
static constexpr char kSilentHandlerPath[] = "/HttpClientSync/test";
static constexpr char kIncompleteBodyPath[] = "/HttpClientSync/incomplete-body";
static constexpr char kCompressedFilePath[] = "/HttpClientSync/compressed-file";
static constexpr char kHttp10BodyPath[] = "/HttpClientSync/http-1.0-body";


class HttpClientSync:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
protected:
    HttpClientSync():
        m_testHttpServer(std::make_unique<TestHttpServer>())
    {
    }

    TestHttpServer* testHttpServer()
    {
        return m_testHttpServer.get();
    }

    nx::utils::Url staticDataUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kStaticDataPath).toUrl();
    }

    nx::utils::Url fileUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kTestFileHttpPath).toUrl();
    }

    nx::utils::Url silentHandlerUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kSilentHandlerPath).toUrl();
    }

    nx::utils::Url incompleteBodyUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kIncompleteBodyPath).toUrl();
    }

    nx::utils::Url compressedBodyUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kCompressedFilePath).toUrl();
    }

    nx::utils::Url http10BodyUrl() const
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(kHttp10BodyPath).toUrl();
    }

    const nx::Buffer& fileBody() const
    {
        return m_fileBody;
    }

private:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
    nx::Buffer m_fileBody;

    virtual void SetUp() override
    {
        ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
            kStaticDataPath,
            kStaticData,
            "application/text"));

        // Generating and saving a temporary file.
        m_fileBody = nx::utils::random::generate(1024).toStdString();
        const auto filePath = testDataDir().toStdString() + "/test";
        std::ofstream f(filePath, std::ios_base::out | std::ios_base::binary);
        ASSERT_TRUE(f.is_open()) << SystemError::getLastOSErrorText();
        f << (std::string_view) m_fileBody;
        f.close();

        ASSERT_TRUE(testHttpServer()->registerFileProvider(
            kTestFileHttpPath,
            filePath,
            kTestFileMimeType));

        ASSERT_TRUE(testHttpServer()->registerRequestProcessorFunc(
            kSilentHandlerPath,
            [](RequestContext requestContext, RequestProcessedHandler handler)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                requestContext.connection->closeConnection(SystemError::connectionReset);
                handler(StatusCode::internalServerError);
            }));

        ASSERT_TRUE(testHttpServer()->registerRequestProcessorFunc(
            kIncompleteBodyPath,
            [](RequestContext requestContext, RequestProcessedHandler handler)
            {
                RequestResult result(StatusCode::ok);
                result.dataSource = std::make_unique<TestBodySource>(
                    "text/plain",
                    "Reported Content-Length of this body is much larger",
                    10000);
                requestContext.connection->setPersistentConnectionEnabled(false);
                handler(std::move(result));
            }));

        ASSERT_TRUE(testHttpServer()->registerRequestProcessorFunc(
            kCompressedFilePath,
            [this](RequestContext requestContext, RequestProcessedHandler handler)
            {
                RequestResult result(StatusCode::ok);
                result.dataSource = std::make_unique<BufferSource>(
                    "application/octet-stream",
                    nx::utils::bstream::gzip::Compressor::compressData(m_fileBody));
                requestContext.response->headers.emplace("Content-Encoding", "gzip");
                handler(std::move(result));
            }));

        ASSERT_TRUE(testHttpServer()->registerRequestProcessorFunc(
            kHttp10BodyPath,
            [this](RequestContext, RequestProcessedHandler handler)
            {
                RequestResult result(StatusCode::ok);
                result.dataSource = std::make_unique<TestBodySource>(
                    "application/octet-stream",
                    m_fileBody,
                    std::nullopt);
                handler(std::move(result));
            }));

        ASSERT_TRUE(testHttpServer()->bindAndListen());
    }
};

TEST_F(HttpClientSync, SimpleTest)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    ASSERT_TRUE(client.doGet(staticDataUrl()));
    ASSERT_TRUE(client.response());
    ASSERT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
    ASSERT_EQ(client.fetchMessageBodyBuffer(), kStaticData);
}

TEST_F(HttpClientSync, http_client_can_be_reused_after_failed_request)
{
    HttpClient client{ssl::kAcceptAnyCertificate};

    // Failing request by response timeout.
    client.setResponseReadTimeout(std::chrono::milliseconds(1));
    ASSERT_FALSE(client.doGet(silentHandlerUrl()));

    // Reusing client.
    client.setResponseReadTimeout(kNoTimeout);
    ASSERT_TRUE(client.doGet(staticDataUrl()));

    // process does not crash.
}

TEST_F(HttpClientSync, FileDownload)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    for (int i = 0; i < 17; ++i)
    {
        ASSERT_TRUE(client.doGet(fileUrl()));
        ASSERT_TRUE(client.response() != nullptr);
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        // Emulating error response from server.
        if (nx::utils::random::number(0, 2) == 0)
            continue;

        nx::Buffer msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

        ASSERT_EQ(fileBody(), msgBody);
    }
}

TEST_F(HttpClientSync, KeepAlive)
{
    static const int TEST_RUNS = 2;

    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        ASSERT_TRUE(client.doGet(fileUrl()));
        ASSERT_TRUE(client.response());
        ASSERT_EQ(client.response()->statusLine.statusCode, nx::network::http::StatusCode::ok);
        nx::Buffer newMsgBody;
        while (!client.eof())
            newMsgBody += client.fetchMessageBodyBuffer();
        if (i == 0)
            msgBody = newMsgBody;
        else
            ASSERT_EQ(msgBody, newMsgBody);
    }
}

TEST_F(HttpClientSync, fetchEntireMessageBody_reads_the_complete_body)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    ASSERT_TRUE(client.doGet(fileUrl()));
    const auto body = client.fetchEntireMessageBody();
    ASSERT_TRUE(body);
    ASSERT_EQ(fileBody(), *body);
}

TEST_F(
    HttpClientSync,
    fetchEntireMessageBody_reads_the_complete_body_terminated_by_connection_closure)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    ASSERT_TRUE(client.doGet(http10BodyUrl()));
    const auto body = client.fetchEntireMessageBody();
    ASSERT_TRUE(body);
    ASSERT_EQ(fileBody(), *body);
}

TEST_F(HttpClientSync, fetchEntireMessageBody_reads_the_complete_compressed_body)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    ASSERT_TRUE(client.doGet(compressedBodyUrl()));
    const auto body = client.fetchEntireMessageBody();
    ASSERT_TRUE(body);
    ASSERT_EQ(fileBody(), *body);
}

TEST_F(HttpClientSync, fetchEntireMessageBody_does_not_report_incomplete_body)
{
    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setResponseReadTimeout(nx::network::kNoTimeout);

    ASSERT_TRUE(client.doGet(incompleteBodyUrl()));
    const auto body = client.fetchEntireMessageBody();
    ASSERT_FALSE(body);
}

} // namespace nx::network::http::test
