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
#include "media_server/serverutil.h"

namespace
{
    const QByteArray UT_SERVER_GUID("{9DBF7579-3235-4910-A66C-0EB5DE8C443B}");
    const int MT_REQUESTS = 10;

}

class MulticastHttpTestWorker: public QObject
{
public:

    enum State
    {
        StateSingleTest,
        StateAuthTest,
        StateTimeoutTest,
        StatePasswordlTest,
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
        m_firstRequest(0),
        newPassword(lit("0")),
        oldPassword(lit("admin"))
    {
        connect(&processor, &MediaServerProcess::started, this, [this]() { m_processorStarted = true; } );
    }

    void setState(State value)
    {
        m_state = value;
        QString stateName;
        switch (m_state) {
            case StateSingleTest:
                stateName = lit("StateSingleTest");
                break;
            case StateAuthTest:
                stateName = lit("StateAuthTest");
                break;
            case StateTimeoutTest:
                stateName = lit("StateTimeoutTest");
                break;
            case StatePasswordlTest:
                stateName = lit("StatePasswordlTest");
                break;
            case StateParralelTest:
                stateName = lit("StateParralelTest");
                break;
            case StateParralelTestInProgress:
                stateName = lit("StateParralelTestInProgress");
                break;
            case StateStopping:
                stateName = lit("StateStopping");
                break;
            default:
                stateName = lit("Unknown");
                break;
        }
        qDebug() << "MulticastHttpTestWorker: goes to state: " << stateName;
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

            setState(StateAuthTest);
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

            setState(StateTimeoutTest);
        }, 3000);
    }

    void callbackTimeout(const QUuid& requestId, QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
    {
        ASSERT_TRUE(m_runningRequestId == requestId);
        ASSERT_TRUE(errCode == QnMulticast::ErrCode::timeout);
        m_runningRequestId = QUuid();
        if (++m_requests == 5)
            setState(StateParralelTest);
        else 
        {
            QnMulticast::Request request;
            request.method = lit("GET");
            request.serverId = QUuid::createUuid(); // missing server
            request.url = QUrl(lit("api/showLog"));
            request.auth.setUser(lit("admin"));
            request.auth.setPassword(lit("12345"));
            using namespace std::placeholders;
            m_runningRequestId = m_client.execRequest(
                request, 
                std::bind(
                    &MulticastHttpTestWorker::callbackTimeout,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3
                ), 
                50
            );
        }
    };

    void doTimeoutTest()
    {
        m_requests = 0;
        QnMulticast::Request request;
        request.method = lit("GET");
        request.serverId = QUuid::createUuid(); // missing server
        request.url = QUrl(lit("api/showLog"));
        request.auth.setUser(lit("admin"));
        request.auth.setPassword(lit("12345"));

        using namespace std::placeholders;
        m_runningRequestId = m_client.execRequest(
            request, 
            std::bind(
                &MulticastHttpTestWorker::callbackTimeout,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            ), 
            50
        );
    }

    void doPasswordTest()
    {
        QnMulticast::Request request;
        request.method = lit("GET");
        request.serverId = QUuid(UT_SERVER_GUID);
        request.url = QUrl(lit("api/configure?password=%1&oldPassword=%2").arg(newPassword).arg(oldPassword));
        request.auth.setUser(lit("admin"));
        request.auth.setPassword(oldPassword);
        
        auto callback = [this](const QUuid& requestId, QnMulticast::ErrCode errCode, const QnMulticast::Response& response)
        {
            m_runningRequestId = QUuid();
            ASSERT_TRUE(errCode == QnMulticast::ErrCode::ok);
            ASSERT_TRUE(response.httpResult == 200); // HTTP OK
            ASSERT_TRUE(response.contentType.toLower().contains("json"));
            ASSERT_TRUE(!response.messageBody.isEmpty());
            QJsonDocument d = QJsonDocument::fromJson(response.messageBody);
            ASSERT_TRUE(!d.isEmpty());
            oldPassword = newPassword;
            if (++m_requests == MT_REQUESTS) 
            {
                ASSERT_EQ(m_firstRequest, MT_REQUESTS);
                m_processor.stopAsync();
                setState(StateStopping);
            }
            newPassword = QString::number(m_requests);
        };
        m_runningRequestId = m_client.execRequest(request, callback, 1000 * 30);
    }

    void doParallelTest()
    {
        setState(StateParralelTestInProgress);
        m_firstRequest = 0;
        m_requests = 0;
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
                if (response.messageBody.contains("\"serverFlags\""))
                    ++m_firstRequest;
                ++m_requests;
                qDebug() << "doParallelTest(). completed: " << m_requests << "of" << MT_REQUESTS * 2;
                if (m_requests == MT_REQUESTS * 2) 
                {
                    ASSERT_EQ(m_firstRequest, MT_REQUESTS);
                    //m_processor.stopAsync();
                    m_requests = 0;
                    setState(StatePasswordlTest);
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
        else if (m_state == StateTimeoutTest)
            doTimeoutTest();
        else if (m_state == StateParralelTest)
            doParallelTest();
        else if (m_state == StatePasswordlTest)
            doPasswordTest();
    }
private:
    QnMulticast::HTTPClient m_client;
    QUuid m_runningRequestId;
    MediaServerProcess& m_processor;
    bool m_processorStarted;
    State m_state;
    int m_requests;
    int m_firstRequest;
    QString newPassword;
    QString oldPassword;
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
    QString eventsDbName = MSSettings::roSettings()->value( "eventsDBFilePath", closeDirPath(getDataDirectory()) + QString(lit("mserver.sqlite")) ).toString();
    QFile::remove(eventsDbName);
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
