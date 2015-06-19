#include "ec2_statictics_reporter.h"

#include "ec2_connection.h"

#include <boost/range/algorithm/count_if.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/synctime.h>

static const uint DEFAULT_TIME_CYCLE = 30 * 24 * 60 * 60; /* secs => about a month */
static const bool DEFAULT_SERVER_AUTH = true;

static const uint MIN_DELAY_RATIO = 30;
static const uint RND_DELAY_RATIO = 50;    /* 50% about 15 days */
static const uint MAX_DELAY_RATIO = MIN_DELAY_RATIO + RND_DELAY_RATIO;

static const uint TIMER_CYCLE = 60000; /* msecs, update state every minute */
static const QString SERVER_API_COMMAND = lit("statserver/api/report");

static const QString ALREADY_IN_PROGRESS = lit("already in progress");
static const QString JUST_INITIATED = lit("just initiated");

namespace ec2
{
    const QString Ec2StaticticsReporter::SR_LAST_TIME = lit("statisticsReportLastTime");
    const QString Ec2StaticticsReporter::SR_TIME_CYCLE = lit("statisticsReportTimeCycle");
    const QString Ec2StaticticsReporter::SR_SERVER_API = lit("statisticsReportServerApi");
    const QString Ec2StaticticsReporter::SYSTEM_ID = lit("systemId");

    const QString Ec2StaticticsReporter::DEFAULT_SERVER_API = lit("http://stats.networkoptix.com");

    // Hardcoded credentials (because of no way to keep it better)
    const QString Ec2StaticticsReporter::AUTH_USER = lit("nx");
    const QString Ec2StaticticsReporter::AUTH_PASSWORD = lit(
                "f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");

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
            const AbstractResourceManagerPtr& resourceManager,
            const AbstractMediaServerManagerPtr& msManager)
        : m_admin(getAdmin(userManager))
        , m_desktopCameraTypeId(getDesktopCameraTypeId(resourceManager))
        , m_msManager(msManager)
        , m_timerDisabled(false)
        , m_timerId(boost::none)
    {
        Q_ASSERT(MAX_DELAY_RATIO <= 100);

        // Just in case assert did not work!
        if (m_admin)
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

    bool Ec2StaticticsReporter::isDefined(const QnMediaServerResourceList &servers)
    {
        /* Returns if any of the server has explicitly defined value. */
        return boost::algorithm::any_of(servers, [](const QnMediaServerResourcePtr& ms) {
            return ms->hasProperty(Qn::STATISTICS_REPORT_ALLOWED);
        });
    }

    bool Ec2StaticticsReporter::isAllowed(const QnMediaServerResourceList &servers)
    {
        int result = 0;
        for (const QnMediaServerResourcePtr &server: servers) {
            if (!server->hasProperty(Qn::STATISTICS_REPORT_ALLOWED))
                continue;
            bool allowedForServer = QnLexical::deserialized(server->getProperty(Qn::STATISTICS_REPORT_ALLOWED), true);
            if (allowedForServer)
                ++result;
            else
                --result;
        }
        return result >= 0;
    }

    bool Ec2StaticticsReporter::isAllowed(const AbstractMediaServerManagerPtr& msManager)
    {
        QnMediaServerResourceList msList;
        if (msManager->getServersSync(QnUuid(), &msList) != ErrorCode::ok)
            return false;
        return isAllowed(msList);
    }

    void Ec2StaticticsReporter::setAllowed(const QnMediaServerResourceList &servers, bool value)
    {
        const QString serializedValue = QnLexical::serialized(value);
        for (const QnMediaServerResourcePtr &server: servers)
            server->setProperty(Qn::STATISTICS_REPORT_ALLOWED, serializedValue);
    }

    QnUserResourcePtr Ec2StaticticsReporter::getAdmin(const AbstractUserManagerPtr& manager)
    {
        QnUserResourceList userList;
        manager->getUsersSync(QnUuid(), &userList);
        for (auto& user : userList)
            if (user->isAdmin())
                return user;

        qFatal("Admin user does not exist");
        NX_LOG(lit("Ec2StaticticsReporter: Admin user does not exist"), cl_logERROR); // In case qFatal didnt work!
        return QnUserResourcePtr();
    }

    QnUuid Ec2StaticticsReporter::getDesktopCameraTypeId(const AbstractResourceManagerPtr& manager)
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

            if (timerId = m_timerId)
                m_timerId = boost::none;
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
        if (!isAllowed(m_msManager))
        {
            NX_LOG(lit("Ec2StaticticsReporter: Automatic report system is disabled"),
                   cl_logDEBUG2);

            // Better luck next time (if report system will be enabled by another mediaserver)
            setupTimer();
            return;
        }
        
        const QDateTime now = qnSyncTime->currentDateTime().toUTC();
        const QDateTime lastTime = QDateTime::fromString(m_admin->getProperty(SR_LAST_TIME), Qt::ISODate);

        const uint timeCycle = secsWithPostfix(m_admin->getProperty(SR_TIME_CYCLE), DEFAULT_TIME_CYCLE);
        const uint maxDelay = timeCycle * MAX_DELAY_RATIO / 100;

        if (!m_plannedReportTime || *m_plannedReportTime > now.addSecs(timeCycle + maxDelay))
        {
            static std::once_flag flag;
            std::call_once(flag, [](){ qsrand((uint)QTime::currentTime().msec()); });

            const auto minDelay = timeCycle * MIN_DELAY_RATIO / 100;
            const auto rndDelay = timeCycle * (static_cast<uint>(qrand()) % RND_DELAY_RATIO) / 100;
            m_plannedReportTime = lastTime.addSecs(timeCycle + minDelay + rndDelay);
            
            NX_LOG(lit("Ec2StaticticsReporter: Last report was at %1, the next planned for %2")
                   .arg(lastTime.isValid() ? lastTime.toString(Qt::ISODate) : lit("NEWER"))
                   .arg(m_plannedReportTime->toString(Qt::ISODate)), cl_logINFO);
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

        m_httpClient->setUserName(AUTH_USER);
        m_httpClient->setUserPassword(AUTH_PASSWORD);

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
        if (httpClient->hasRequestSuccesed())
        {
            NX_LOG(lit("Ec2StaticticsReporter: Statistics report successfully sent to %1")
                   .arg(httpClient->url().toString()), cl_logINFO);
            
            const auto now = qnSyncTime->currentDateTime().toUTC();
            m_plannedReportTime = boost::none;

            m_admin->setProperty(SR_LAST_TIME, now.toString(Qt::ISODate));
            propertyDictionary->saveParams(m_admin->getId());
        }
        else
        {
            NX_LOG(lit("Ec2StaticticsReporter: Could not send report to %1, retry after random delay...")
                   .arg(httpClient->url().toString()), cl_logWARNING);
        }

        setupTimer();
    }
}
