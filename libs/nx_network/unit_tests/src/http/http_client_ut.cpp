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

//TODO #akolesnikov refactor these tests using HttpClientSync

TEST(HttpClientTest, DISABLED_KeepAlive2)
{
    nx::utils::Url url("http://192.168.0.1:7001/ec2/testConnection");
    url.setUserName("admin");
    url.setPassword("123");
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response());
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        nx::Buffer newMsgBody;
        while (!client.eof())
            newMsgBody += client.fetchMessageBodyBuffer();
        if (i == 0)
            msgBody = newMsgBody;
        else
            ASSERT_EQ(msgBody, newMsgBody);
    }
}

TEST(HttpClientTest, DISABLED_KeepAlive3)
{
    nx::utils::Url url("http://192.168.0.194:7001/ec2/events?guid=%7Be7209f3e-9ebe-6ebb-3e99-e5acd61c228c%7D&runtime-guid=%7B83862a97-b7b8-4dbc-bb8f-64847f23e6d5%7D&system-identity-time=0");
    url.setUserName("admin");
    url.setPassword("123");
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.addAdditionalHeader("NX-EC-SYSTEM-NAME", "ak_ec_2.3");
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response());
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        nx::Buffer newMsgBody;
        while (!client.eof())
            newMsgBody += client.fetchMessageBodyBuffer();
        if (i == 0)
            msgBody = newMsgBody;
        else
            ASSERT_EQ(msgBody, newMsgBody);
    }
}

TEST(HttpClientTest, DISABLED_mjpgRetrieval)
{
    nx::utils::Url url("http://192.168.0.92/mjpg/video.mjpg");
    url.setUserName("root");
    url.setPassword("root");
    static const int TEST_RUNS = 100;

    HttpClient client{ssl::kAcceptAnyCertificate};
    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response());
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        nx::Buffer newMsgBody;
        const int readsCount = nx::utils::random::number(10, 20);
        for (int i = 0; i < readsCount; ++i)
            newMsgBody += client.fetchMessageBodyBuffer();
        continue;
    }
}

TEST(HttpClientTest, DISABLED_fileDownload2)
{
    const nx::utils::Url url("http://192.168.0.1/girls/Candice-Swanepoel-127.jpg");
    static const int THREADS = 10;
    static const std::chrono::seconds TEST_DURATION(15);

    std::vector<nx::utils::thread> threads;
    std::vector<std::pair<bool, std::shared_ptr<nx::network::http::HttpClient>>> clients;
    std::mutex mtx;
    std::atomic<bool> isTerminated(false);

    auto threadFunc = [url, &clients, &mtx, &isTerminated]()
    {
        while (!isTerminated)
        {
            nx::Buffer msgBody;

            std::unique_lock<std::mutex> lk(mtx);
            int pos = nx::utils::random::number<int>(0, (int)clients.size() - 1);
            while (clients[pos].first)
                pos = (pos + 1) % clients.size();
            auto client = clients[pos].second;
            clients[pos].first = true;
            lk.unlock();

            ASSERT_TRUE(client->doGet(url));
            if ((nx::utils::random::number(0, 2) != 0) && !client->eof() && (client->response() != nullptr))
            {
                const auto statusCode = client->response()->statusLine.statusCode;
                ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);
                nx::Buffer newMsgBody;
                while (!client->eof())
                    newMsgBody += client->fetchMessageBodyBuffer();
            }

            lk.lock();
            if (clients[pos].second == client)
                clients[pos].first = false;
        }
    };

    for (int i = 0; i < THREADS; ++i)
        clients.emplace_back(false, std::make_shared<HttpClient>(ssl::kAcceptAnyCertificate));

    for (int i = 0; i < THREADS; ++i)
        threads.emplace_back(nx::utils::thread(threadFunc));

    const auto beginTime = std::chrono::steady_clock::now();
    while (beginTime + TEST_DURATION > std::chrono::steady_clock::now())
    {
        std::unique_lock<std::mutex> lk(mtx);
        const int pos = nx::utils::random::number<int>(0, (int)clients.size() - 1);
        auto client = clients[pos].second;
        lk.unlock();

        client->pleaseStop();
        client.reset();

        lk.lock();
        clients[pos].second = std::make_shared<HttpClient>(ssl::kAcceptAnyCertificate);
        clients[pos].first = false;
        lk.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    isTerminated = true;

    for (int i = 0; i < THREADS; ++i)
        threads[i].join();
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
