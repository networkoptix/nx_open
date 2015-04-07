#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"
#include "core/resource_management/resource_properties.h"

static const uint SECS_A_DAY = 24*60*60;

namespace ec2
{
    // Hardcoded credentials (because of no way to keep it better)
    static const QString AUTH_USER = lit("nx");
    static const QString AUTH_PASSWORD = lit("f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");

    // Hardcoded defaults
    Ec2StaticticsReporter::Constants Ec2StaticticsReporter::c_constants =
    {
        /* timeCycle    = */ 30 * SECS_A_DAY,   /* about a month */
        /* maxDelay     = */ 10 * SECS_A_DAY,   /* just 10 days */

        // TODO: fix as soon as we have actual public server
        /* serverApi    = */ QLatin1String("http://stats.networkoptix.com/statserver/api/reportStatistics"),
        /* serverAuth   = */ true,
    };

    const QString Ec2StaticticsReporter::SR_ALLOWED = lit("statisticsReportAllowed");
    const QString Ec2StaticticsReporter::SR_LAST_TIME = lit("statisticsReportLastTime");
    const QString Ec2StaticticsReporter::SYSTEM_ID = lit("systemId");

    static const QString ALREADY_IN_PROGRESS = lit("already in progress");
    static const QString JUST_INITIATED = lit("just initiated");

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

        NX_LOG(lit("Ec2StaticticsReporter: Can not get %1 resource type, using null")
               .arg(QnResourceTypePool::desktopCameraTypeName), cl_logWARNING);
        return QnUuid();
    }

    static QString toTimeString(uint secs)
    {
        auto days = secs / SECS_A_DAY, rest = secs % SECS_A_DAY;
        return lit("%1 days %2").arg(days).arg(rest ?
            QTime().addSecs(rest).toString() : lit("sharp"));
    }

    Ec2StaticticsReporter::Ec2StaticticsReporter(
            const AbstractUserManagerPtr& userManager,
            const AbstractResourceManagerPtr& resourceManager)
        : m_admin(getAdmin(userManager))
        , m_desktopCameraTypeId(getDesktopCameraTypeId(resourceManager))
        , m_timerDisabled(false)
    {
        // Do not allow to set delay longer than cycle itself to avoid funny situation
        // when tester sets cycle 10min and still waits for report a month :)
        if (c_constants.maxDelay > c_constants.timeCycle)
        {
            NX_LOG(lit("Ec2StaticticsReporter: maxDalay=%1 is bigger, timeCycle=%2, reducing: timeCycle=%2")
                   .arg(toTimeString(c_constants.maxDelay))
                   .arg(toTimeString(c_constants.timeCycle)), cl_logWARNING);

            c_constants.maxDelay = c_constants.timeCycle;
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter: maxDelay=%1, timeCycle=%2")
                   .arg(toTimeString(c_constants.maxDelay))
                   .arg(toTimeString(c_constants.timeCycle)), cl_logDEBUG1);
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
            if (res != ErrorCode::ok) return res;
            for (auto& ms : mediaservers) outData->mediaservers.push_back(std::move(ms));
        } {
            ApiCameraDataExList cameras;
            auto res = QnDbManager::instance()->doQuery(nullptr, cameras);
            if (res != ErrorCode::ok) return res;

            for (ApiCameraDataEx& cam : cameras)
                if (cam.typeId != m_desktopCameraTypeId)
                    outData->cameras.push_back(std::move(cam));
        } {
            auto res = QnDbManager::instance()->doQuery(nullptr, outData->clients);
            if (res != ErrorCode::ok) return res;
        } {
            ApiLicenseDataList licenses;
            auto res = QnDbManager::instance()->doQuery(nullptr, licenses);
            if (res != ErrorCode::ok) return res;
            for (auto& lic : licenses) outData->licenses.push_back(std::move(lic));
        } {
			ApiBusinessRuleDataList bRules;
            auto res = QnDbManager::instance()->doQuery(nullptr, bRules);
            if (res != ErrorCode::ok) return res;
            for (auto& br : bRules) outData->businessRules.push_back(std::move(br));
        }

        NX_LOG(lit("Ec2StaticticsReporter: Collected: %1 mediaserver(s), %2 camera(s) and %3 client(s) in with systemId=%4")
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
        outData->url = c_constants.serverApi;

        {
            QMutexLocker lk(&m_mutex);
            if (m_timerDisabled)
            {
                outData->status = ALREADY_IN_PROGRESS;
                return ErrorCode::ok;
            }
        }

        NX_LOG(lit("Ec2StaticticsReporter: Request for statistics report to %1, for systemId=%2")
               .arg(outData->url)
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

        // Add random delay
        delay += static_cast<uint>(qrand()) % c_constants.maxDelay;
        {
            QMutexLocker lk(&m_mutex);
            if (m_timerDisabled)
                return;

            m_timerId = TimerManager::instance()->addTimer(
                    std::bind(&Ec2StaticticsReporter::timerEvent, this), delay);
        }
        NX_LOG(lit("Ec2StaticticsReporter: Planned statistics report time is %1")
               .arg(QDateTime::currentDateTime().addSecs(delay).toString(Qt::ISODate)),
               cl_logDEBUG1);
    }

    void Ec2StaticticsReporter::timerEvent()
    {
        if (m_admin->getProperty(SR_ALLOWED).toInt() == 0)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Automatic report system is disabled"),
                   cl_logDEBUG1);

            // Better luck next time (if report system will be enabled by another mediaserver)
            setupTimer();
            return;
        }

        const auto last = m_admin->getProperty(SR_LAST_TIME);
        if (!last.isEmpty())
        {
            const auto now = QDateTime::currentDateTime().toTime_t();
            const auto plan = QDateTime::fromString(last, Qt::ISODate)
                .addSecs(c_constants.timeCycle).toTime_t();

            if (plan > now)
            {
                setupTimer(plan - now);
                return;
            }
        }

        NX_LOG(lit("Ec2StaticticsReporter: Last report had been executed %1 initiating it now %2")
               .arg(last.isEmpty() ? "NEWER" : last)
               .arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
               cl_logDEBUG2);

        if (initiateReport() != ErrorCode::ok)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1, retry after random delay...")
                   .arg(c_constants.serverApi), cl_logWARNING);

            setupTimer();
        }
    }

    ErrorCode Ec2StaticticsReporter::initiateReport()
    {
        ApiSystemStatistics data;
        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not collect data: %1")
                   .arg(toString(res)), cl_logWARNING);
            return res;
        }

        m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
        connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        if (c_constants.serverAuth) // auth can be disabled by command line
        {
            m_httpClient->setUserName(AUTH_USER);
            m_httpClient->setUserPassword(AUTH_PASSWORD);
        }

        const auto format = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        if (!m_httpClient->doPost(QUrl(c_constants.serverApi), format, QJson::serialized(data)))
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1")
                   .arg(c_constants.serverApi), cl_logWARNING);
            return ErrorCode::failure;
        }

        NX_LOG(lit("Ec2StaticticsReporter: Sending statistics asynchronously"), cl_logDEBUG1);
        return ErrorCode::ok;
    }

    void Ec2StaticticsReporter::finishReport(nx_http::AsyncHttpClientPtr httpClient)
    {
        if (httpClient->state() == nx_http::AsyncHttpClient::sFailed)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1, retry after random delay...")
                   .arg(c_constants.serverApi), cl_logWARNING);

            setupTimer();
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter: Statistics report successfully sent to %1")
                   .arg(c_constants.serverApi), cl_logINFO);

            const auto now = QDateTime::currentDateTime();
            m_admin->setProperty(SR_LAST_TIME, now.toString(Qt::ISODate));
            propertyDictionary->saveParams(m_admin->getId());

            setupTimer(c_constants.timeCycle);
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
