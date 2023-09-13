// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

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

static const int kWaitTimeoutMs = 1000;

} // namespace

namespace nx::network::http {

class HttpClientPoolTest: public ::testing::Test
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

    int doGet(
        ClientPool* clientPool,
        const nx::utils::Url& url,
        std::function<void (QSharedPointer<ClientPool::Context> context)> completionFunc)
    {
        ClientPool::ContextPtr context(new ClientPool::Context(QnUuid(), nullptr));
        context->request.method = Method::get;
        context->request.url = url;
        context->completionFunc = completionFunc;
        return clientPool->sendRequest(std::move(context));
    }

protected:
    nx::network::SocketGlobals::InitGuard m_guard;
    std::unique_ptr<TestHttpServer> m_testHttpServer;
    std::unique_ptr<TestHttpServer> m_testHttpServer2;
    QEventLoop m_loop;
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

    std::unique_ptr<ClientPool> httpPool(new ClientPool());

    nx::Mutex mutex;
    nx::WaitCondition waitCond;
    std::atomic_int requestsFinished = 0;

    auto completionFunc =
        [&](ClientPool::ContextPtr context)
        {
            ASSERT_TRUE(context->hasResponse());
            ASSERT_EQ(context->response.statusLine.statusCode, StatusCode::ok);
            QByteArray msgBody = context->response.messageBody;
            ASSERT_TRUE(msgBody.startsWith("SimpleTest"));
            if (msgBody == "SimpleTest2")
                ASSERT_TRUE(context->response.contentType == QByteArray("application/text2"));
            else
                ASSERT_TRUE(context->response.contentType ==  QByteArray("application/text"));

            NX_MUTEX_LOCKER lock(&mutex);
            ++requestsFinished;
            waitCond.wakeAll();
        };

    static const int kRequests = 100;
    for (int i = 0; i < kRequests; ++i)
    {
        doGet(httpPool.get(), url, completionFunc);
        doGet(httpPool.get(), url2, completionFunc);
    }

    // All events are arriving in separate thread.
    // We just need to check requestsFinished value.
    NX_MUTEX_LOCKER lock(&mutex);
    while (requestsFinished < kRequests * 2)
        waitCond.wait(&mutex, kWaitTimeoutMs);
}

TEST_F(HttpClientPoolTest, GeneralNegativeTest)
{
    // Testing negative scenarios, like requesting wrong URL.
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

    ClientPool httpPool;

    nx::Mutex mutex;
    nx::WaitCondition waitCond;
    int requestsFinished = 0;

    static const int kRequests = 10;
    const nx::utils::Url wrongUrl(lit("nothttp://127.0.0.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));

    const nx::utils::Url wrongPathUrl(lit("http://127.0.0.1:%1/wrong_test2")
        .arg(testHttpServer2()->serverAddress().port));

    // We expect all requests to fail
    auto completionFunc =
        [&](ClientPool::ContextPtr context)
        {
            EXPECT_GE(context->getTimeElapsed().count(), 0);

            if (context->getUrl() == wrongUrl)
            {
                // We are expecting there are no proper response at all.
                EXPECT_FALSE(context->hasResponse());
                EXPECT_NE(context->systemError, SystemError::noError);
            }
            else if (context->getUrl() == wrongPathUrl)
            {
                // There should be some response with 404.
                EXPECT_EQ(context->systemError, SystemError::noError);
                EXPECT_TRUE(context->hasResponse());
                EXPECT_EQ(context->getStatusCode(), StatusCode::notFound);
            }

            NX_MUTEX_LOCKER lock(&mutex);
            ++requestsFinished;
            waitCond.wakeAll();
        };

    int requestsSent = 0;
    for (int i = 0; i < kRequests; ++i)
    {
        if (doGet(&httpPool, wrongUrl, completionFunc))
            ++requestsSent;
        if (doGet(&httpPool, wrongPathUrl, completionFunc))
            ++requestsSent;
    }

    // All events are arriving in separate thread.
    // We just need to check requestsFinished value.
    NX_MUTEX_LOCKER lock(&mutex);
    while (requestsFinished < kRequests * 2)
        ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));

    ASSERT_TRUE(httpPool.size() == 0);
}

TEST_F(HttpClientPoolTest, terminateTest)
{
    static const int kWaitTimeoutMs = 1000;
    ASSERT_TRUE(testHttpServer()->registerRequestProcessorFunc(
        "/test",
        [&](RequestContext, RequestProcessedHandler handler)
        {
            // This is a durty huck which causes server to respond with delay so client pool have
            // enough time to cancel requests.
            auto delayedTimer = std::make_shared<nx::network::aio::Timer>(
                nx::network::SocketGlobals::aioService().getCurrentAioThread());
            delayedTimer->start(
                std::chrono::milliseconds(kWaitTimeoutMs / 2),
                [delayedTimer, handler = std::move(handler)]()
                {
                    handler(StatusCode::ok);
                    delayedTimer->pleaseStopSync();
                });
        }));

    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const nx::utils::Url url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));

    nx::Mutex mutex;
    nx::WaitCondition waitCond;
    std::atomic<int> requestsCanceled = 0;
    std::atomic<int> requestsCompleted = 0;

    QSet<int> requests;
    QSet<int> requestsToBeTerminated;

    {
        auto httpPool = std::make_unique<ClientPool>();
        auto completionFunc =
            [&](ClientPool::ContextPtr context)
            {
                NX_MUTEX_LOCKER lock(&mutex);
                if (requestsToBeTerminated.contains(context->handle))
                {
                    // Should have no response.
                    ASSERT_TRUE(context->isCanceled());
                    ++requestsCanceled;
                }
                else
                {
                    ASSERT_TRUE(context->hasResponse());
                    ++requestsCompleted;
                }
                waitCond.wakeAll();
            };

        static const int kRequests = 20;

        {
            NX_MUTEX_LOCKER lock(&mutex);
            for (int i = 0; i < kRequests; ++i)
            {
                requests << doGet(httpPool.get(), url, completionFunc);
            }
            ASSERT_TRUE(httpPool->size() > 0);
        }

        // Terminate some requests.
        // NOTE: In threaded mode some requests could be already served.
        for (int handle: requests)
        {
            if (nx::utils::random::number() % 2 == 1)
            {
                {
                    NX_MUTEX_LOCKER lock(&mutex);
                    requestsToBeTerminated.insert(handle);
                }
                // NOTE: We should not lock 'mutex' around 'terminate' call, or we get a deadlock
                // with requestIsDone handler.
                if (!httpPool->terminate(handle))
                {
                    NX_MUTEX_LOCKER lock(&mutex);
                    requestsToBeTerminated.remove(handle);
                }
            }
        }

        QElapsedTimer limiter;
        limiter.start();
        // All events are arriving in separate thread.
        // We just need to wait until all requests are served.
        {
            NX_MUTEX_LOCKER lock(&mutex);
            while (httpPool->size() > 0 && !limiter.hasExpired(kWaitTimeoutMs*10))
                waitCond.wait(&mutex, kWaitTimeoutMs);
        }

        ASSERT_EQ(requestsToBeTerminated.size(), requestsCanceled);

        // Checking that all requests are sent.
        ASSERT_TRUE(httpPool->size() == 0);

        // Ensure all requests callbacks are processed completely.
        httpPool.reset();

        ASSERT_EQ(requestsCompleted + requestsCanceled, kRequests);
    }
}

} // namespace nx::network::http
