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
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace http {

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

private:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};

TEST_F(HttpClientServerTest, SimpleTest)
{
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test",
        "SimpleTest",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    nx::network::http::HttpClient client;
    const nx::utils::Url url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));

    ASSERT_TRUE(client.doGet(url));
    ASSERT_TRUE(client.response());
    ASSERT_EQ(client.response()->statusLine.statusCode, nx::network::http::StatusCode::ok);
    ASSERT_EQ(client.fetchMessageBodyBuffer(), "SimpleTest");
}

/*!
This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST_F(HttpClientServerTest, KeepAliveConnection)
{
}

TEST_F(HttpClientServerTest, FileDownload)
{
#if 1
    QFile f(":/content/Candice-Swanepoel-915.jpg");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const auto fileBody = f.readAll();
    f.close();
#else
    const QByteArray fileBody(
        "01xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "02xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "03xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "04xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "05xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "06xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "07xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "08xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "09xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "13xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "14xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "15xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "16xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "17xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "18xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "19xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "20xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "21xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "22xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "23xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "24xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "25xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "26xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "27xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "28xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "29xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    );
#endif

    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test.jpg",
        fileBody,
        "image/jpeg"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const nx::utils::Url url(lit("http://127.0.0.1:%1/test.jpg")
        .arg(testHttpServer()->serverAddress().port));
    nx::network::http::HttpClient client;
    client.setMessageBodyReadTimeoutMs(3000);

    for (int i = 0; i<77; ++i)
    {
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response() != nullptr);
        ASSERT_EQ(nx::network::http::StatusCode::ok, client.response()->statusLine.statusCode);
        //emulating error response from server
        if (nx::utils::random::number(0, 2) == 0)
            continue;
        nx::network::http::BufferType msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

#if 0
        int pos = 0;
        for (pos = 0; pos < std::min<int>(fileBody.size(), msgBody.size()); ++pos)
            if (fileBody[pos] != msgBody[pos])
                break;
#endif

        ASSERT_EQ(fileBody.size(), msgBody.size());
        ASSERT_EQ(fileBody, msgBody);
    }
}

TEST_F(HttpClientServerTest, KeepAlive)
{
    static const int TEST_RUNS = 2;

    QFile f(":/content/Candice-Swanepoel-915.jpg");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const auto fileBody = f.readAll();
    f.close();

    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test.jpg",
        fileBody,
        "image/jpeg"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const nx::utils::Url url(lit("http://127.0.0.1:%1/test.jpg")
        .arg(testHttpServer()->serverAddress().port));

    nx::network::http::HttpClient client;

    nx::Buffer msgBody;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        ASSERT_TRUE(client.doGet(url));
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
