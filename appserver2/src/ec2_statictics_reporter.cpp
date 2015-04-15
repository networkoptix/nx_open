#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"
#include "core/resource_management/resource_properties.h"

static const uint DEFAULT_TIME_CYCLE = 30 * 24 * 60 * 60; /* secs => about a month */
static const bool DEFAULT_SERVER_AUTH = true;

static const uint DELAY_RATIO = 50;    /* 50% about 15 days */
static const uint TIMER_CYCLE = 60000; /* msecs, update state every minute */
static const QString DATE_FORMAT = lit("yyyy-MM-dd hh:mm:ss");
static const QString SERVER_API_COMMAND = lit("reportStatistics");

static const QString ALREADY_IN_PROGRESS = lit("already in progress");
static const QString JUST_INITIATED = lit("just initiated");

namespace ec2
{
    const QString Ec2StaticticsReporter::SR_ALLOWED = lit("statisticsReportAllowed");
    const QString Ec2StaticticsReporter::SR_LAST_TIME = lit("statisticsReportLastTime");
    const QString Ec2StaticticsReporter::SR_TIME_CYCLE = lit("statisticsReportTimeCycle");
    const QString Ec2StaticticsReporter::SR_SERVER_API = lit("statisticsReportServerApi");
    const QString Ec2StaticticsReporter::SR_SERVER_NO_AUTH = lit("statisticsReportServerNoAuth");
    const QString Ec2StaticticsReporter::SYSTEM_ID = lit("systemId");

    const QString Ec2StaticticsReporter::DEFAULT_SERVER_API = lit(
                "http://stats.networkoptix.com/stats/api");

    // Hardcoded credentials (because of no way to keep it better)
    const QString Ec2StaticticsReporter::AUTH_USER = lit("nx");
    const QString Ec2StaticticsReporter::AUTH_PASSWORD = lit(
                "f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");

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

    static uint secsWithPostfix(const QString& str, uint defaultValue)
    {
        qlonglong secs;
        bool ok(true);

        if (str.endsWith(lit("m"), Qt::CaseInsensitive)) 
            secs = str.left(str.length() - 1).toLongLong(&ok) * 60; // minute
        else
        if (str.endsWith(lit("h"), Qt::CaseInsensitive)) 
            secs = str.left(str.length() - 1).toLongLong(&ok) * 60 * 60; // hour
        else 
        if (str.endsWith(lit("d"), Qt::CaseInsensitive)) 
            secs = str.left(str.length() - 1).toLongLong(&ok) * 60 * 60 * 24; // day
        else
        if (str.endsWith(lit("M"), Qt::CaseInsensitive)) 
            secs = str.left(str.length() - 1).toLongLong(&ok) * 60 * 60 * 24 * 30; // month
        else
            secs = str.toLongLong(&ok); // seconds
        
        return (ok && secs) ? static_cast<uint>(secs) : defaultValue;
    }

    Ec2StaticticsReporter::Ec2StaticticsReporter(
            const AbstractUserManagerPtr& userManager,
            const AbstractResourceManagerPtr& resourceManager)
        : m_admin(getAdmin(userManager))
        , m_desktopCameraTypeId(getDesktopCameraTypeId(resourceManager))
        , m_timerDisabled(false)
    {
        setupTimer();
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        removeTimer();
    }

    ErrorCode Ec2StaticticsReporter::collectReportData(std::nullptr_t, ApiSystemStatistics* const outData)
    {
        if(!dbManager) return ErrorCode::ioError;

        ErrorCode res;
        #define dbManager_queryOrReturn(ApiType, name) \
            ApiType name; \
            if ((res = dbManager->doQuery(nullptr, name)) != ErrorCode::ok) \
                return res;

        dbManager_queryOrReturn(ApiMediaServerDataExList, mediaservers);
        for (auto& ms : mediaservers) outData->mediaservers.push_back(std::move(ms));
            
        dbManager_queryOrReturn(ApiCameraDataExList, cameras);
        for (ApiCameraDataEx& cam : cameras)
            if (cam.typeId != m_desktopCameraTypeId)
                outData->cameras.push_back(std::move(cam));

        if ((res = dbManager->doQuery(nullptr, outData->clients)) != ErrorCode::ok)
            return res;

        dbManager_queryOrReturn(ApiLicenseDataList, licenses);
        for (auto& lic : licenses) outData->licenses.push_back(std::move(lic));

        dbManager_queryOrReturn(ApiBusinessRuleDataList, bRules);
        for (auto& br : bRules) outData->businessRules.push_back(std::move(br));
        
        #undef dbManager_queryOrReturn

        outData->systemId = getOrCreateSystemId();
        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData)
    {
        outData->systemId = getOrCreateSystemId();
        {
            QMutexLocker lk(&m_mutex);
            if (m_timerDisabled)
            {
                outData->status = ALREADY_IN_PROGRESS;
                return ErrorCode::ok;
            }
        }

        removeTimer();
        outData->status = JUST_INITIATED;
        return initiateReport(&outData->url);
    }

    void Ec2StaticticsReporter::setupTimer()
    {
        QMutexLocker lk(&m_mutex);
        if (!m_timerDisabled)
            m_timerId = TimerManager::instance()->addTimer(
                std::bind(&Ec2StaticticsReporter::timerEvent, this), TIMER_CYCLE);
    }

    void Ec2StaticticsReporter::removeTimer()
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

        {
            QMutexLocker lk(&m_mutex);
            m_timerDisabled = false;
        }
    }

    void Ec2StaticticsReporter::timerEvent()
    {
        if (m_admin->getProperty(SR_ALLOWED).toInt() == 0)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Automatic report system is disabled"),
                   cl_logDEBUG2);

            // Better luck next time (if report system will be enabled by another mediaserver)
            setupTimer();
            return;
        }
        
        const QDateTime now = QDateTime::currentDateTime();
        const QDateTime lastTime = QDateTime::fromString(m_admin->getProperty(SR_LAST_TIME), DATE_FORMAT);

        const uint timeCycle = secsWithPostfix(m_admin->getProperty(SR_TIME_CYCLE), DEFAULT_TIME_CYCLE);
        const uint maxDelay = timeCycle * DELAY_RATIO / 100;

        if (!m_plannedReportTime || *m_plannedReportTime > now.addSecs(timeCycle + maxDelay))
        {
            static std::once_flag flag;
            std::call_once(flag, [](){ qsrand((uint)QTime::currentTime().msec()); });

            const auto delay = static_cast<uint>(qrand()) % maxDelay;
            m_plannedReportTime = lastTime.addSecs(timeCycle + delay);
            
            NX_LOG(lit("Ec2StaticticsReporter: Last report was at %1, the next planned for %2")
                   .arg(lastTime.isValid() ? lastTime.toString(DATE_FORMAT) : lit("NEWER"))
                   .arg(m_plannedReportTime->toString(DATE_FORMAT)), cl_logINFO);
        }

        if (*m_plannedReportTime > now)
        {
            // Let's try a little later
            setupTimer();
            return;
        }
        
        initiateReport();
    }

    ErrorCode Ec2StaticticsReporter::initiateReport(QString* reportApi)
    {
        ApiSystemStatistics data;
        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not collect data, error: %1")
                   .arg(toString(res)), cl_logWARNING);
            return res;
        }

        m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
        connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        if (m_admin->getProperty(SR_SERVER_NO_AUTH).toInt() == 0)
        {
            m_httpClient->setUserName(AUTH_USER);
            m_httpClient->setUserPassword(AUTH_PASSWORD);
        }

        const QString configApi = m_admin->getProperty(SR_SERVER_API);
        const QString serverApi = configApi.isEmpty() ? DEFAULT_SERVER_API : configApi;
        const QUrl url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);
        const auto format = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        if (!m_httpClient->doPost(url, format, QJson::serialized(data)))
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1")
                   .arg(url.toString()), cl_logWARNING);
            return ErrorCode::failure;
        }

        NX_LOG(lit("Ec2StaticticsReporter: Sending statistics asynchronously to %1")
               .arg(url.toString()), cl_logDEBUG1);

        if (reportApi) *reportApi = url.toString();
        return ErrorCode::ok;
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

    void Ec2StaticticsReporter::finishReport(nx_http::AsyncHttpClientPtr httpClient)
    {
        if (httpClient->state() == nx_http::AsyncHttpClient::sFailed)
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1, retry after random delay...")
                   .arg(httpClient->url().toString()), cl_logWARNING);
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter: Statistics report successfully sent to %1")
                   .arg(httpClient->url().toString()), cl_logINFO);
            
            m_plannedReportTime = boost::none;
            m_admin->setProperty(SR_LAST_TIME, QDateTime::currentDateTime().toString(DATE_FORMAT));
            propertyDictionary->saveParams(m_admin->getId());
        }
        
        setupTimer();
    }
}
