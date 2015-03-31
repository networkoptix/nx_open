#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"
#include "core/resource_management/resource_properties.h"

static const QString REPORT_ALLOWED = "reportAllowed";
static const QString REPORT_LAST_TIME = "reportLastTime";
static const QString SYSTEM_ID = "systemId";

static const QString ALREADY_IN_PROGRESS = "already in progress";
static const QString JUST_INITIATED = "just innitiated";

// Paramitrize
static const uint REPORT_CYCLE = 30*24*60*60; /* about a month */
static const uint REPORT_MAX_DELAY = REPORT_CYCLE / 4; /* about a week */
static const QString REPORT_SERVER = "http://localhost:5000/api/reportStatistics";

namespace ec2
{
    static QnUserResourcePtr getAdmin(const AbstractUserManagerPtr& manager)
    {
        QnUserResourceList userList;
        manager->getUsersSync(QnUuid(), &userList);
        for (auto& user : userList)
            if (user->isAdmin())
                return user;

        qFatal("Can not get user admin");
		return QnUserResourcePtr();
    }

    static QnUuid getDesktopCameraTypeId(const AbstractResourceManagerPtr& manager)
    {
        QnResourceTypeList typesList;
        manager->getResourceTypesSync(&typesList);
        for (auto& rType : typesList)
            if (rType->getName() == QnResourceTypePool::desktopCameraTypeName)
                return rType->getId();

        NX_LOG(lit("Ec2StaticticsReporter. Can not get %1 resouce type, using null")
               .arg(QnResourceTypePool::desktopCameraTypeName), cl_logWARNING);
        return QnUuid();
    }

    Ec2StaticticsReporter::Ec2StaticticsReporter(
            const AbstractUserManagerPtr& userManager,
            const AbstractResourceManagerPtr& resourceManager)
        : m_admin(getAdmin(userManager))
        , m_desktopCameraTypeId(getDesktopCameraTypeId(resourceManager))
        , m_timerDisabled(false)
    {
        if (m_admin->getProperty(REPORT_ALLOWED).toInt() == 0)
        {
            NX_LOG(lit("Ec2StaticticsReporter. Automatic peport system is disabled"),
                   cl_logDEBUG1);
            return;
        }

        setupTimer();
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        boost::optional<quint64> timerId;
        {
            QMutexLocker lk(&m_mutex);
            m_timerDisabled = true;
            timerId = m_timerId;
        }

        if (timerId)
            TimerManager::instance()->joinAndDeleteTimer(*timerId);

        if (auto client = m_httpClient)
            client->terminate();
    }

    ErrorCode Ec2StaticticsReporter::collectReportData(
            std::nullptr_t, ApiSystemStatistics* const outData)
    {
        if( !QnDbManager::instance() )
            return ErrorCode::ioError;

        outData->systemId = getOrCreateSystemId();
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
                if (cam.typeId == m_desktopCameraTypeId)
                    outData->clients.push_back(cam);
                else
                    outData->cameras.push_back(cam);
        }

        NX_LOG(lit("Ec2StaticticsReporter. Collected: %1 mediaserver(s), %2 camera(s) and %3 client(s) in with systemId=%4")
               .arg(outData->mediaservers.size())
               .arg(outData->cameras.size())
               .arg(outData->clients.size())
               .arg(outData->systemId.toString()), cl_logDEBUG1);
        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(
            std::nullptr_t, ApiStatisticsServerInfo* const outData)
    {
        outData->systemId = getOrCreateSystemId();
        outData->url = REPORT_SERVER;

        {
            QMutexLocker lk(&m_mutex);
            if (m_timerDisabled)
            {
                outData->status = ALREADY_IN_PROGRESS;
                return ErrorCode::ok;
            }
        }

        NX_LOG(lit("Ec2StaticticsReporter. Request for statistics report to %1, for systemId=%2")
               .arg(REPORT_SERVER)
               .arg(outData->systemId.toString()), cl_logDEBUG1);

        boost::optional<quint64> timerId;
        {
            QMutexLocker lk(&m_mutex);
            m_timerDisabled = true;
            timerId = m_timerId;
        }

        if (timerId)
            TimerManager::instance()->joinAndDeleteTimer(*timerId);

        if (auto client = m_httpClient)
            client->terminate();

        {
            QMutexLocker lk(&m_mutex);
            m_timerDisabled = false;
        }

        // NOTE: current design provides possibility, when previous report has been
        //       just sent, and we're going to send another one
        outData->status = JUST_INITIATED;
        return initiateReport();
    }

    void Ec2StaticticsReporter::setupTimer(uint delay)
    {
        static std::once_flag flag;
        std::call_once(flag, [](){ qsrand((uint)QTime::currentTime().msec()); });

        // Add randome delay
        delay += static_cast<uint>(qrand()) % REPORT_MAX_DELAY;
        {
            QMutexLocker lk(&m_mutex);
            if (m_timerDisabled)
                return;

            m_timerId = TimerManager::instance()->addTimer(
                    std::bind(&Ec2StaticticsReporter::timerEvent, this), delay);
        }
        NX_LOG(lit("Ec2StaticticsReporter. Planned statistics report time is %1")
               .arg(QDateTime::currentDateTime().addSecs(delay).toString(Qt::ISODate)),
               cl_logDEBUG1);
    }

    void Ec2StaticticsReporter::timerEvent()
    {
        const auto last = m_admin->getProperty(REPORT_LAST_TIME);
        if (!last.isEmpty())
        {
            const auto now = QDateTime::currentDateTime().toTime_t();
            const auto plan = QDateTime::fromString(last, Qt::ISODate)
                    .addSecs(REPORT_CYCLE).toTime_t();

            if (plan > now)
            {
                setupTimer(plan - now);
                return;
            }
        }

        NX_LOG(lit("Ec2StaticticsReporter. Last report had been executed %1 initiating it now %2")
               .arg(last.isEmpty() ? "NEWER" : last)
               .arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
               cl_logDEBUG2);

        if (initiateReport() != ErrorCode::ok)
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not send report to %1, retry after randome delay...")
                   .arg(REPORT_SERVER), cl_logWARNING);

            setupTimer();
        }

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
        connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        const auto format = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        if (!m_httpClient->doPost(QUrl(REPORT_SERVER), format, QJson::serialized(data)))
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
        if( httpClient->state() == nx_http::AsyncHttpClient::sFailed )
        {
            NX_LOG(lit("Ec2StaticticsReporter. Could not send report to %1, retry after randome delay...")
                   .arg(REPORT_SERVER), cl_logWARNING);

            setupTimer();
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter. Statistics report sucessfuly sent to %1")
                   .arg(REPORT_SERVER), cl_logINFO);

            const auto now = QDateTime::currentDateTime();
            m_admin->setProperty(REPORT_LAST_TIME, now.toString(Qt::ISODate));
            propertyDictionary->saveParams(m_admin->getId());

            setupTimer(REPORT_CYCLE);
        }
    }

    QnUuid Ec2StaticticsReporter::getOrCreateSystemId()
    {
        auto systemId = m_admin->getProperty(SYSTEM_ID);
        if (!systemId.isEmpty())
            return QnUuid(systemId);

        auto newId = QnUuid::createUuid();
        m_admin->setProperty(SYSTEM_ID, newId.toString());
        propertyDictionary->saveParams(m_admin->getId());
        return newId;
    }
}
