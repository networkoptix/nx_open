#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <api/http_client_pool.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>
#include <utils/common/sleep.h>

namespace {
    static const int kWaitTimeoutMs = 1000 * 10;
}

namespace nx {
namespace network {
namespace http {

class HttpClientPoolTest
:
    public ::testing::Test
{
public:
    HttpClientPoolTest()
    :
        m_testHttpServer(std::make_unique<TestHttpServer>()),
        m_testHttpServer2(std::make_unique<TestHttpServer>())
    {
    }

    TestHttpServer* testHttpServer()
    {
        return m_testHttpServer.get();
    }

    TestHttpServer* testHttpServer2()
    {
        return m_testHttpServer2.get();
    }

protected:
    nx::network::SocketGlobals::InitGuard m_guard;
    std::unique_ptr<TestHttpServer> m_testHttpServer;
    std::unique_ptr<TestHttpServer> m_testHttpServer2;
};

TEST_F(HttpClientPoolTest, GeneralTest)
{
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test",
        "SimpleTest",
        "application/text"));
    ASSERT_TRUE(testHttpServer2()->registerStaticProcessor(
        "/test2",
        "SimpleTest2",
        "application/text2"));

    ASSERT_TRUE(testHttpServer()->bindAndListen());
    ASSERT_TRUE(testHttpServer2()->bindAndListen());

    const nx::utils::Url url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));
    const nx::utils::Url url2(lit("http://127.0.0.1:%1/test2")
        .arg(testHttpServer2()->serverAddress().port));


    std::unique_ptr<nx::network::http::ClientPool> httpPool(new nx::network::http::ClientPool());

    QByteArray expectedResponse;

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsFinished = 0;

    QObject::connect(
        httpPool.get(), &nx::network::http::ClientPool::done,
        httpPool.get(),
        [&](int /*handle*/, nx::network::http::AsyncHttpClientPtr client)
    {
        ASSERT_FALSE(client->failed());
        ASSERT_EQ(client->response()->statusLine.statusCode, nx::network::http::StatusCode::ok);
        QByteArray msgBody = client->fetchMessageBodyBuffer();
        ASSERT_TRUE(msgBody.startsWith("SimpleTest"));
        if (msgBody == "SimpleTest2")
            ASSERT_TRUE(client->contentType() == QByteArray("application/text2"));
        else
            ASSERT_TRUE(client->contentType() ==  QByteArray("application/text"));

        QnMutexLocker lock(&mutex);
        ++requestsFinished;
        waitCond.wakeAll();
    }, Qt::DirectConnection);

    static const int kRequests = 100;
    for (int i = 0; i < kRequests; ++i)
    {
        httpPool->doGet(url);
        httpPool->doGet(url2);
    }

    QnMutexLocker lock(&mutex);
    while (requestsFinished < kRequests * 2)
        ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
}

TEST_F(HttpClientPoolTest, terminateTest)
{
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test",
        "SimpleTest",
        "application/text"));

    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const nx::utils::Url url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));

    std::unique_ptr<nx::network::http::ClientPool> httpPool(new nx::network::http::ClientPool());

    QByteArray expectedResponse;

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsFinished = 0;
    QSet<int> requests;

    QObject::connect(
        httpPool.get(), &nx::network::http::ClientPool::done,
        httpPool.get(),
        [&](int /*handle*/, nx::network::http::AsyncHttpClientPtr client)
    {
        QnMutexLocker lock(&mutex);
        client->fetchMessageBodyBuffer();
        ++requestsFinished;
        waitCond.wakeAll();
    }, Qt::DirectConnection);

    static const int kWaitTimeoutMs = 1000 * 10;
    static const int kRequests = 100;

    {
        QnMutexLocker lock(&mutex);
        for (int i = 0; i < kRequests; ++i)
        {
            requests << httpPool->doGet(url);
        }
        ASSERT_TRUE(httpPool->size() > 0);
    }

    // terminate some requests
    for (int handle: requests)
    {
        if (nx::utils::random::number() % 2 == 1)
            httpPool->terminate(handle);
    }

    QElapsedTimer waitTimer;
    waitTimer.restart();
    while (!waitTimer.hasExpired(kWaitTimeoutMs) && httpPool->size() > 0)
        QnSleep::msleep(10);

    ASSERT_TRUE(httpPool->size() == 0);
}

} // namespace nx
} // namespace network
} // namespace http
