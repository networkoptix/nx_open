#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"

static const QString REPORT_ALLOWED = "reportAllowed";
static const QString REPORT_LAST_TIME = "reportLastTime";

// Paramitrize
static const uint REPORT_CYCLE = 30*24*60*60;
static const QString REPORT_SERVER = "http;//localhost/reportSystemStatistics";

namespace ec2
{
    Ec2StaticticsReporter::Ec2StaticticsReporter(AbstractECConnection& connection)
        : m_connection(connection)
        , m_admin(getAdmin())
        , m_thread(nullptr)
        , m_destoy(false)
    {
        if (!m_admin)
        {
            // TODO: log the error
            return;
        }

        if (m_admin->getProperty(REPORT_ALLOWED).toInt() == 0)
        {
            // Report system is disabled
            return;
        }

        checkForReportTime();
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        QMutexLocker lk(&m_mutex);
        m_destoy = true;

        if (m_timer)
        {
            lk.unlock();
            TimerManager::instance()->joinAndDeleteTimer(*m_timer);
            lk.relock();
        }

        if (m_thread)
        {
            lk.unlock();
            m_thread->wait();
            delete m_thread;
            lk.relock();
        }
    }

    Ec2StaticticsReporter::ReportThread::ReportThread(Ec2StaticticsReporter& reporter)
        : m_reporter(reporter)
    {
    }

    void Ec2StaticticsReporter::ReportThread::sendJsonReport(QJsonValue array)
    {
        nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
        connect(*httpClient, &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::ReportThread::done, Qt::DirectConnection);

        auto format = Qn::serializationFormatToHttpContentType(JsonFormat);
        if (!httpClient->doPost(QUrl(REPORT_SERVER), format, QJson::serialized(array)))
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not send report to %1").
                   arg(url.toString()), cl_logWARNING);
        }
    }

    void Ec2StaticticsReporter::ReportThread::run()
    {
        QJsonArray jsonServers;
        QnMediaServerResourceList serverList;
        m_reporter.m_connection.getMediaServerManager()->getServersSync(QnUuid(), &serverList);
        for(QnMediaServerResource& server : serverList)
        {
            QJsonObject jsonServer;
            jsonServer.insert("id", server.getId());
            jsonServer.insert("system_id", server.getId());

            jsonServer.
        }

        QJsonArray jsonCameras;
        QJsonArray jsonClients;
        QnVirtualCameraResourceList cameraList;
        m_reporter.m_connection.getCameraManager()->getCamerasSync(QnUuid(), &cameraList);
        for(QnVirtualCameraResource& camera : cameraList)
        {
            if (/* camera is client */)
            jsonServerMap[camera.getParentId()]
        }

        sendJsonReport(array);
    }

    void Ec2StaticticsReporter::ReportThread::done(nx_http::AsyncHttpClientPtr httpClient)
    {
        auto now = QDateTime::currentDateTime().toTime_t();
        m_reporter.m_admin.setProperty(REPORT_LAST_TIME, QString::number(now));
        m_reporter.checkForReportTime();
    }

    QnUserResource Ec2StaticticsReporter::getAdmin()
    {
        QnUserResourceList userList;
        connection.getUserManager()->getUsersSync(QnUuid(), &userList);
        for (auto& user : userList)
            if (user.isAdmin())
                return user;

        qFatal("Can not get user admin");
    }

    void Ec2StaticticsReporter::checkForReportTime(qint64 /*timerId*/)
    {
        {
            QMutexLocker lk(&m_mutex);
            if (destroy) return;
        }

        auto now = QDateTime::currentDateTime().toTime_t();
        auto lastReport = m_admin->getProperty(REPORT_LAST_TIME).toUInt();
        if (now >= lastReport + REPORT_CYCLE)
        {
            QMutexLocker lk(&m_mutex);
            m_timer = boost::none;

            // Statistics shouldnt be in process right now
            Q_ASSERT(!(m_thread && m_thread->isRunning()));

            m_thread = new ReportThread(m_connection);
            m_thread->start();
        }
        else
        {
            QMutexLocker lk(&m_mutex);
            m_timer = TimerManager::instance()->addTimer(
                std::bind(this, Ec2StaticticsReporter::checkForReportTime),
                lastReport + REPORT_CYCLE < now);
        }
    }
}
