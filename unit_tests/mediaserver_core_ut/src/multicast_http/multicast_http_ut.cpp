#include "multicast_http_ut.h"
#include "media_server_process.h"
#include "platform/platform_abstraction.h"
#include "utils/common/long_runnable.h"
#include "media_server/media_server_module.h"
#include "core/multicast/multicast_http_client.h"

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

namespace
{
    const QByteArray UT_SERVER_GUID("{9DBF7579-3235-4910-A66C-0EB5DE8C443B}");
    const int MT_REQUESTS = 20;

}

class MulticastHttpTestWorker: public QObject
{
public:

    enum State
    {
        StateSingleTest,
        StateAuthTest,
        StateParralelTest,
        StateParralelTestInProgress,
        StateStopping
    };

    MulticastHttpTestWorker(MediaServerProcess& processor): 
        m_client("Server Unit Tests"),
        m_processor(processor),
        m_processorStarted(false),
        m_state(StateSingleTest),
        m_requests(0),
        m_firstRequest(0)
    {
        connect(&processor, &MediaServerProcess::started, this, [this]() { m_processorStarted = true; } );
    }

    void doSingleTest()
    {
        QnMulticast::Request request;
        request.method = lit("GET");
        request.serverId = QUuid(UT_SERVER_GUID);
        request.url = QUrl(lit("api/moduleInformation"));
        request.auth.setUser(lit("admin"));
        request.auth.setPassword(lit("admin"));

        m_runningRequestId = m_client.execRequest(request, [this](const QUuid& requestId, QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
        {
            ASSERT_TRUE(m_runningRequestId == requestId);
            ASSERT_TRUE(errCode == QnMulticast::ErrCode::ok);
            ASSERT_TRUE(response.httpResult == 200); // HTTP OK
            ASSERT_TRUE(response.contentType.toLower().contains("json"));
            ASSERT_TRUE(!response.messageBody.isEmpty());
            QJsonDocument d = QJsonDocument::fromJson(response.messageBody);
            ASSERT_TRUE(!d.isEmpty());
            m_runningRequestId = QUuid();

            m_state = StateAuthTest;
        }, 3000);
    }

    void doAuthTest()
    {
        QnMulticast::Request request;
        request.method = lit("GET");
        request.serverId = QUuid(UT_SERVER_GUID);
        request.url = QUrl(lit("api/showLog"));
        request.auth.setUser(lit("admin"));
        request.auth.setPassword(lit("12345"));

        m_runningRequestId = m_client.execRequest(request, [this](const QUuid& requestId, QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
        {
            ASSERT_TRUE(m_runningRequestId == requestId);
            ASSERT_TRUE(errCode == QnMulticast::ErrCode::ok);
            ASSERT_TRUE(response.httpResult == 401); // HTTP Unauthorized
            m_runningRequestId = QUuid();

            m_state = StateParralelTest;
        }, 3000);
    }

    void doParallelTest()
    {
        m_state = StateParralelTestInProgress;
        QnMulticast::Request request1;
        request1.method = lit("GET");
        request1.serverId = QUuid(UT_SERVER_GUID);
        request1.url = QUrl(lit("api/moduleInformation"));
        request1.auth.setUser(lit("admin"));
        request1.auth.setPassword(lit("admin"));

        QnMulticast::Request request2(request1);
        request1.url = QUrl(lit("/api/auditLog"));
        for (int i = 0; i < MT_REQUESTS; ++i)
        {
            auto callback = [this](const QUuid& requestId, QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
            {
                ASSERT_TRUE(errCode == QnMulticast::ErrCode::ok);
                ASSERT_TRUE(response.httpResult == 200); // HTTP OK
                ASSERT_TRUE(response.contentType.toLower().contains("json"));
                ASSERT_TRUE(!response.messageBody.isEmpty());
                QJsonDocument d = QJsonDocument::fromJson(response.messageBody);
                ASSERT_TRUE(!d.isEmpty());
                if (response.messageBody.contains("\"flags\""))
                    ++m_firstRequest;
                if (++m_requests == MT_REQUESTS * 2) 
                {
                    ASSERT_EQ(m_firstRequest, MT_REQUESTS);
                    m_processor.stopAsync();
                    m_state = StateStopping;
                }
            };

            m_client.execRequest(request1, callback, 1000 * 30);
            m_client.execRequest(request2, callback, 1000 * 30);
        }
    }

    void at_timer()
    {
        if (!m_processorStarted)
            return;
        if (m_processor.getTcpPort() == 0)
            return; // wait for TCP listener will start

        if (!m_runningRequestId.isNull())
            return;
        
        if (m_state == StateSingleTest)
            doSingleTest();
        else if (m_state == StateAuthTest)
            doAuthTest();
        else if (m_state == StateParralelTest)
            doParallelTest();
    }
private:
    QnMulticast::HTTPClient m_client;
    QUuid m_runningRequestId;
    MediaServerProcess& m_processor;
    bool m_processorStarted;
    State m_state;
    int m_requests;
    int m_firstRequest;
};

TEST(MulticastHttpTest, main)
{
    //QApplication::instance()->args();
    int argc = 2;
    char* argv[] = {"", "-e" };

    QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
    QScopedPointer<QnLongRunnablePool> runnablePool(new QnLongRunnablePool());
    QScopedPointer<QnMediaServerModule> module(new QnMediaServerModule());
    MSSettings::roSettings()->setValue(lit("serverGuid"), UT_SERVER_GUID);
    MSSettings::roSettings()->setValue(lit("removeDbOnStartup"), lit("1"));
    MSSettings::roSettings()->setValue(lit("systemName"), QnUuid::createUuid().toString()); // random value
    MSSettings::roSettings()->setValue(lit("port"), 0);
    MediaServerProcess mserverProcessor(argc, argv);

    QTimer timer;
    MulticastHttpTestWorker worker(mserverProcessor);
    QObject::connect(&timer, &QTimer::timeout, &worker, &MulticastHttpTestWorker::at_timer);
    timer.start(100);

    mserverProcessor.run();

    ASSERT_TRUE(1);
}
