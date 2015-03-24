#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"

static const QString REPORT_ALLOWED = "reportAllowed";
static const QString REPORT_LAST_TIME = "reportLastTime";

// Paramitrize
static const uint REPORT_CYCLE = 30*24*60*60; /* about a month */
static const uint REPORT_MAX_DELAY = REPORT_CYCLE / 4; /* about a week */
static const QString REPORT_SERVER = "http://localhost:5000/api/reportStatistics";

namespace ec2
{
    Ec2StaticticsReporter::Ec2StaticticsReporter(AbstractECConnection& connection)
        : m_connection(connection)
        , m_admin(getAdmin())
        , m_desktopCamera(getResourceTypeIdByName(QnResourceTypePool::desktopCameraTypeName))
    {
        if (m_admin->getProperty(REPORT_ALLOWED).toInt() == 0)
        {
            NX_LOG(lit("Ec2StaticticsReporter. Automatic peport system is disabled"),
                   cl_logDEBUG1);
            return;
        }

        if (m_desktopCamera.isNull())
            NX_LOG(lit("Ec2StaticticsReporter. Can not get %1 resouce type, using null")
                   .arg(QnResourceTypePool::desktopCameraTypeName), cl_logWARNING);

        const auto now = QDateTime::currentDateTime().toTime_t();
        const auto lastReport = m_admin->getProperty(REPORT_LAST_TIME).toUInt();
        const auto plannedReport = lastReport + REPORT_CYCLE;

        setUpTimer(std::max(now, plannedReport));
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        {
            QMutexLocker lk(&m_mutex);
            if (m_timerId)
                TimerManager::instance()->joinAndDeleteTimer(*m_timerId);
        }

        if (m_httpClient)
            m_httpClient.get()->terminate();
    }

    ErrorCode Ec2StaticticsReporter::collectReportData(
            std::nullptr_t, ApiSystemStatistics* const outData)
    {
        if( !QnDbManager::instance() )
            return ErrorCode::ioError;

        {
            ApiMediaServerDataExList mediaservers;
            auto res = QnDbManager::instance()->doQuery(nullptr, mediaservers);
            if (res != ErrorCode::ok)
                return res;

            for (auto& ms : mediaservers)
                outData->mediaservers.push_back(ms);
        }
        {
            ApiCameraDataExList cameras;
            auto res = QnDbManager::instance()->doQuery(nullptr, cameras);
            if (res != ErrorCode::ok)
                return res;

            for (ApiCameraDataEx& cam : cameras)
                if (cam.typeId == m_desktopCamera)
                    outData->clients.push_back(cam);
                else
                    outData->cameras.push_back(cam);
        }

        NX_LOG(lit("Ec2StaticticsReporter. Collected: "
                   "%1 mediaservers, %2 cameras and %3 clients")
               .arg(outData->mediaservers.size())
               .arg(outData->cameras.size())
               .arg(outData->clients.size()), cl_logDEBUG1);
        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(
            std::nullptr_t, ApiStatisticsServerInfo* const outData)
    {
        outData->url = REPORT_SERVER;
        NX_LOG(lit("Ec2StaticticsReporter. Request for statistics report to %1")
               .arg(REPORT_SERVER), cl_logDEBUG1);

        {
            QMutexLocker lk(&m_mutex);
            if (m_timerId)
                TimerManager::instance()->joinAndDeleteTimer(*m_timerId);
        }

        if (m_httpClient) return ErrorCode::ok; // already in progress

        return initiateReport();
    }

    void Ec2StaticticsReporter::setUpTimer(uint reportTime)
    {
        static std::once_flag flag;
        std::call_once(flag, [](){ qsrand((uint)QTime::currentTime().msec()); });

        const uint randomDelay = static_cast<uint>(qrand()) % REPORT_MAX_DELAY;
        const uint plannedTime = reportTime + randomDelay;

        NX_LOG(lit("Ec2StaticticsReporter. Planned statistics report time is %1")
               .arg(QDateTime::fromTime_t(plannedTime).toString()), cl_logDEBUG1);

        QMutexLocker lk(&m_mutex);
        m_timerId = TimerManager::instance()->addTimer(
                std::bind(&Ec2StaticticsReporter::initiateReport, this), plannedTime);
    }

    ErrorCode Ec2StaticticsReporter::initiateReport()
    {
        ApiSystemStatistics data;
        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not collect data: %1")
                   .arg(toString(res)), cl_logWARNING);
            return res;
        }

        m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
        connect(m_httpClient.get().get(), &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        const auto format = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        if (!m_httpClient.get()->doPost(QUrl(REPORT_SERVER), format, QJson::serialized(data)))
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not send report to %1")
                   .arg(REPORT_SERVER), cl_logWARNING);
            return ErrorCode::failure;
        }

        NX_LOG(lit("Ec2StaticticsReporter. Sending statistics async"), cl_logDEBUG1);
        return ErrorCode::ok;
    }

    void Ec2StaticticsReporter::finishReport(nx_http::AsyncHttpClientPtr httpClient)
    {
        m_httpClient = boost::none;

        const auto now = QDateTime::currentDateTime().toTime_t();
        if( httpClient->state() == nx_http::AsyncHttpClient::sFailed )
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not send report to %1")
                   .arg(REPORT_SERVER), cl_logWARNING);

            // retry after randome dalay
            setUpTimer(now);
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter. Statistics report sucessfuly sent to %1")
                   .arg(REPORT_SERVER), cl_logINFO);

            // plan for a next report
            m_admin->setProperty(REPORT_LAST_TIME, QString::number(now));
            setUpTimer(now + REPORT_CYCLE);
        }
    }

    QnUserResourcePtr Ec2StaticticsReporter::getAdmin()
    {
        QnUserResourceList userList;
        m_connection.getUserManager()->getUsersSync(QnUuid(), &userList);
        for (auto& user : userList)
            if (user->isAdmin())
                return user;

        qFatal("Can not get user admin");
    }

    QnUuid Ec2StaticticsReporter::getResourceTypeIdByName(const QString& name)
    {
        QnResourceTypeList typesList;
        m_connection.getResourceManager()->getResourceTypesSync(&typesList);
        for (auto& rType : typesList)
            if (rType->getName() == name)
                return rType->getId();

        return QnUuid(); // null
    }
}
