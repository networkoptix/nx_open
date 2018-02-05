#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include <QtCore/QFile>

#include <gtest/gtest.h>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace http {

static const char* const kStaticDataPath = "/test";
static const char* const kStaticData = "SimpleTest";
static const char* const kTestFileHttpPath = "/test.jpg";
static const char* const kTestFilePath = ":/content/Candice-Swanepoel-915.jpg";

class HttpClientServerTest:
    public ::testing::Test
{
protected:
    HttpClientServerTest():
        m_testHttpServer(new TestHttpServer())
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

    const QByteArray& fileBody() const
    {
        return m_fileBody;
    }

private:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
    QByteArray m_fileBody;

    virtual void SetUp() override
    {
        ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
            kStaticDataPath,
            kStaticData,
            "application/text"));

        QFile f(kTestFilePath);
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        m_fileBody = f.readAll();
        f.close();

        ASSERT_TRUE(testHttpServer()->registerFileProvider(
            kTestFileHttpPath,
            kTestFilePath,
            "image/jpeg"));

        ASSERT_TRUE(testHttpServer()->bindAndListen());
    }
};

TEST_F(HttpClientServerTest, SimpleTest)
{
    nx::network::http::HttpClient client;
    client.setResponseReadTimeoutMs(nx::network::kNoTimeout.count());

    ASSERT_TRUE(client.doGet(staticDataUrl()));
    ASSERT_TRUE(client.response());
    ASSERT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
    ASSERT_EQ(client.fetchMessageBodyBuffer(), kStaticData);
}

TEST_F(HttpClientServerTest, FileDownload)
{
    nx::network::http::HttpClient client;
    client.setResponseReadTimeoutMs(nx::network::kNoTimeout.count());

    for (int i = 0; i < 17; ++i)
    {
        ASSERT_TRUE(client.doGet(fileUrl()));
        ASSERT_TRUE(client.response() != nullptr);
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        // Emulating error response from server.
        if (nx::utils::random::number(0, 2) == 0)
            continue;

        nx::network::http::BufferType msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

        ASSERT_EQ(fileBody(), msgBody);
    }
}

TEST_F(HttpClientServerTest, KeepAlive)
{
    static const int TEST_RUNS = 2;

    nx::network::http::HttpClient client;
    client.setResponseReadTimeoutMs(nx::network::kNoTimeout.count());

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

//TODO #ak refactor these tests using HttpClientServerTest

TEST(HttpClientTest, DISABLED_KeepAlive2)
{
    nx::utils::Url url("http://192.168.0.1:7001/ec2/testConnection");
    url.setUserName("admin");
    url.setPassword("123");
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        nx::network::http::HttpClient client;
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
        nx::network::http::HttpClient client;
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

    nx::Buffer msgBody;
    nx::network::http::HttpClient client;
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
        clients.emplace_back(false, std::make_shared<nx::network::http::HttpClient>());

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
        clients[pos].second = std::make_shared<nx::network::http::HttpClient>();
        clients[pos].first = false;
        lk.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    isTerminated = true;

    for (int i = 0; i < THREADS; ++i)
        threads[i].join();
}

} // namespace nx
} // namespace network
} // namespace http
