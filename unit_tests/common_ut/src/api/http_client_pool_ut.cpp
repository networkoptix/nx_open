/**********************************************************
* 26 dec 2014
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <nx/utils/std/cpp14.h>
#include <utils/media/custom_output_stream.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <api/http_client_pool.h>



namespace nx_http {

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
        "application/text"));

    ASSERT_TRUE(testHttpServer()->bindAndListen());
    ASSERT_TRUE(testHttpServer2()->bindAndListen());

    const QUrl url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));
    const QUrl url2(lit("http://127.0.0.1:%1/test2")
        .arg(testHttpServer2()->serverAddress().port));


    std::unique_ptr<nx_http::ClientPool> httpPool(new nx_http::ClientPool());

    QByteArray expectedResponse;

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsFinished = 0;

    QObject::connect(
        httpPool.get(), &nx_http::ClientPool::done,
        httpPool.get(),
        [&](int handle, nx_http::AsyncHttpClientPtr client)
    {
        ASSERT_FALSE(client->failed());
        ASSERT_EQ(client->response()->statusLine.statusCode, nx_http::StatusCode::ok);
        ASSERT_TRUE(client->fetchMessageBodyBuffer().startsWith("SimpleTest"));

        QnMutexLocker lock(&mutex);
        ++requestsFinished;
        waitCond.wakeAll();
    }, Qt::DirectConnection);

    static const int kWaitTimeoutMs = 1000 * 10;
    for (int i = 0; i < 100; ++i)
    {
        httpPool->doGet(url);
        httpPool->doGet(url2);
    }

    QnMutexLocker lock(&mutex);
    while (requestsFinished < 200)
        ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
}


} // namespace nx_http
