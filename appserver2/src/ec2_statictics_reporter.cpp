#include "ec2_statictics_reporter.h"

#include <QtCore/QCollator>

#include <api/global_settings.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/random.h>

#include <utils/common/synctime.h>
#include <utils/common/app_info.h>

#include "ec2_connection.h"

static const uint DEFAULT_TIME_CYCLE = 30 * 24 * 60 * 60; /* secs => about a month */
static const bool DEFAULT_SERVER_AUTH = true;

static const uint MIN_DELAY_RATIO = 30;
static const uint RND_DELAY_RATIO = 50;    /* 50% about 15 days */
static const uint MAX_DELAY_RATIO = MIN_DELAY_RATIO + RND_DELAY_RATIO;

static const uint SEND_AFTER_UPDATE_TIME = 3 * 60 * 60 * 1000; // 3h
static const uint TIMER_CYCLE = 60 * 1000; /* msecs, update state every minute */
static const uint TIMER_CYCLE_MAX = 24 * 60 * 60 * 1000; /* msecs, once a day at least */

static const QString SERVER_API_COMMAND = lit("statserver/api/report");

namespace ec2
{
    const QString Ec2StaticticsReporter::DEFAULT_SERVER_API = lit("http://stats.networkoptix.com");

    // Hardcoded credentials (because of no way to keep it better)
    const QString Ec2StaticticsReporter::AUTH_USER = lit("nx");
    const QString Ec2StaticticsReporter::AUTH_PASSWORD = lit(
                "f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");

    Ec2StaticticsReporter::Ec2StaticticsReporter(
            const AbstractResourceManagerPtr& resourceManager,
            const AbstractMediaServerManagerPtr& msManager)
        :
         m_desktopCameraTypeId(getDesktopCameraTypeId(resourceManager))
        , m_msManager(msManager)
        , m_firstTime(true)
        , m_timerCycle(TIMER_CYCLE)
        , m_timerDisabled(false)
        , m_timerId(boost::none)
    {
        NX_CRITICAL(MAX_DELAY_RATIO <= 100);
        setupTimer();
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        removeTimer();
    }

    ErrorCode Ec2StaticticsReporter::collectReportData(std::nullptr_t, ApiSystemStatistics* const outData)
    {
        if(!detail::QnDbManager::instance() || !detail::QnDbManager::instance()->isInitialized())
            return ErrorCode::ioError;

        ErrorCode res;
        #define dbManager_queryOrReturn(ApiType, name) \
            ApiType name; \
            if ((res = dbManager(Qn::kSystemAccess).doQuery(nullptr, name)) != ErrorCode::ok) \
                return res;

        dbManager_queryOrReturn(ApiMediaServerDataExList, mediaservers);
        for (auto& ms : mediaservers) outData->mediaservers.push_back(std::move(ms));

        dbManager_queryOrReturn(ApiCameraDataExList, cameras);
        for (ApiCameraDataEx& cam : cameras)
            if (cam.typeId != m_desktopCameraTypeId)
                outData->cameras.push_back(std::move(cam));

        if ((res = dbManager(Qn::kSystemAccess).doQuery(QnUuid(), outData->clients)) != ErrorCode::ok)
            return res;

        dbManager_queryOrReturn(ApiLicenseDataList, licenses);
        for (auto& license : licenses)
        {
            QnLicense qnLicense(license.licenseBlock);
            ApiLicenseStatistics statLicense(std::move(license));
            statLicense.validation = qnLicense.validationInfo();
            outData->licenses.push_back(std::move(statLicense));
        }

        dbManager_queryOrReturn(ApiBusinessRuleDataList, bRules);
        for (auto& br : bRules) outData->businessRules.push_back(std::move(br));

        if ((res = dbManager(Qn::kSystemAccess).doQuery(nullptr, outData->layouts)) != ErrorCode::ok)
            return res;

        dbManager_queryOrReturn(ApiUserDataList, users);
        for (auto& u : users) outData->users.push_back(std::move(u));

        #undef dbManager_queryOrReturn

        outData->systemId = getOrCreateSystemId();
        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData)
    {
        removeTimer();
        outData->systemId = getOrCreateSystemId();
        outData->status = lit("initiated");
        return initiateReport(&outData->url);
    }

    QnUuid Ec2StaticticsReporter::getDesktopCameraTypeId(const AbstractResourceManagerPtr& manager)
    {
        QnResourceTypeList typesList;
        manager->getResourceTypesSync(&typesList);
        for (auto& rType : typesList)
            if (rType->getName() == QnResourceTypePool::kDesktopCameraTypeName)
                return rType->getId();

        NX_LOG(lm("Can not get %1 resource type, using null")
               .arg(QnResourceTypePool::kDesktopCameraTypeName), cl_logWARNING);
        return QnUuid();
    }

    void Ec2StaticticsReporter::setupTimer()
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_timerDisabled)
        {
            m_timerId = nx::utils::TimerManager::instance()->addTimer(
                std::bind(&Ec2StaticticsReporter::timerEvent, this),
                std::chrono::milliseconds(m_timerCycle));

            NX_LOGX(lm("Timer is set with delay %1").arg(m_timerCycle), cl_logDEBUG1);
        }
    }

    void Ec2StaticticsReporter::removeTimer()
    {
        boost::optional<quint64> timerId;
        {
            QnMutexLocker lk(&m_mutex);
            m_timerDisabled = true;

            if (timerId = m_timerId)
                m_timerId = boost::none;
        }

        if (timerId)
            nx::utils::TimerManager::instance()->joinAndDeleteTimer(*timerId);

        if (auto client = m_httpClient)
            client->pleaseStopSync();

        {
            QnMutexLocker lk(&m_mutex);
            m_timerDisabled = false;
        }
    }

    void Ec2StaticticsReporter::timerEvent()
    {
        if (!qnGlobalSettings->isInitialized())
        {
            /* Try again. */
            setupTimer();
            return;
        }

        if (!qnGlobalSettings->isStatisticsAllowed()
            || qnGlobalSettings->isNewSystem())
        {
            NX_LOGX(lm("Automatic report system is disabled"), cl_logINFO);

            // Better luck next time (if report system will be enabled by another mediaserver)
            setupTimer();
            return;
        }

        const QDateTime now = qnSyncTime->currentDateTime().toUTC();
        if (plannedReportTime(now) <= now)
        {
            if (initiateReport() == ErrorCode::ok)
                return;
        }

        // let's retry a little later
        setupTimer();
    }

    QDateTime Ec2StaticticsReporter::plannedReportTime(const QDateTime& now)
    {
        if (m_firstTime)
        {
            m_firstTime = false;
            const auto reportedVersion = qnGlobalSettings->statisticsReportLastVersion();
            const auto currentVersion = QnAppInfo::applicationFullVersion();

            QCollator collator;
            collator.setNumericMode(true);
            if (collator.compare(currentVersion, reportedVersion) > 0)
            {
                const uint timeCycle = nx::utils::parseTimerDuration(
                    qnGlobalSettings->statisticsReportTimeCycle(),
                    std::chrono::seconds(SEND_AFTER_UPDATE_TIME)).count() / 1000;

                m_plannedReportTime = now.addSecs(nx::utils::random::number(
                      timeCycle * MIN_DELAY_RATIO / 100,
                      timeCycle * MAX_DELAY_RATIO / 100));

                NX_LOGX(lm("Last reported version is '%1' while running '%2', plan early report for %3")
                    .arg(reportedVersion).arg(currentVersion)
                    .arg(m_plannedReportTime->toString(Qt::ISODate)), cl_logINFO);

                return *m_plannedReportTime;
            }
        }

        const uint timeCycle = nx::utils::parseTimerDuration(
            qnGlobalSettings->statisticsReportTimeCycle(),
            std::chrono::seconds(DEFAULT_TIME_CYCLE)).count() / 1000;

        const uint minDelay = timeCycle * MIN_DELAY_RATIO / 100;
        const uint maxDelay = timeCycle * MAX_DELAY_RATIO / 100;
        if (!m_plannedReportTime || *m_plannedReportTime > now.addSecs(maxDelay))
        {
            const QDateTime lastTime = qnGlobalSettings->statisticsReportLastTime();
            m_plannedReportTime = (lastTime.isValid() ? lastTime : now).addSecs(
                nx::utils::random::number<uint>(minDelay, maxDelay));

            NX_LOGX(lm("Last report was at %1, the next planned for %2")
               .arg(lastTime.isValid() ? lastTime.toString(Qt::ISODate) : lit("NEWER"))
               .arg(m_plannedReportTime->toString(Qt::ISODate)), cl_logINFO);
        }

        return *m_plannedReportTime;
    }

    ErrorCode Ec2StaticticsReporter::initiateReport(QString* reportApi)
    {
        ApiSystemStatistics data;
        data.reportInfo.number = qnGlobalSettings->statisticsReportLastNumber();
        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_LOGX(lm("Could not collect data, error: %1")
                   .arg(toString(res)), cl_logWARNING);
            return res;
        }

        m_httpClient = nx_http::AsyncHttpClient::create();
        connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        m_httpClient->setUserName(AUTH_USER);
        m_httpClient->setUserPassword(AUTH_PASSWORD);

        const QString configApi = qnGlobalSettings->statisticsReportServerApi();
        const QString serverApi = configApi.isEmpty() ? DEFAULT_SERVER_API : configApi;
        const QUrl url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);
        const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        m_httpClient->doPost(url, contentType, QJson::serialized(data));

        NX_LOGX(lm("Sending statistics asynchronously to %1")
               .arg(url.toString(QUrl::RemovePassword)), cl_logDEBUG1);

        if (reportApi)
            *reportApi = url.toString();

        return ErrorCode::ok;
    }

    QnUuid Ec2StaticticsReporter::getOrCreateSystemId()
    {
        const auto systemName = qnCommon->localSystemName();
        const auto systemNameForId = qnGlobalSettings->systemNameForId();
        const auto systemId = qnGlobalSettings->systemId();
        qDebug() << "system id" << systemId.toString();

        if (systemNameForId == systemName // system name was not changed
            && !systemId.isNull())       // and systemId is already generated
        {
            return QnUuid(systemId);
        }

        const auto newId = QnUuid::createUuid();
        qnGlobalSettings->setSystemId(newId);
        qnGlobalSettings->setSystemNameForId(systemName);
        qnGlobalSettings->synchronizeNow();

        return newId;
    }

    void Ec2StaticticsReporter::finishReport(nx_http::AsyncHttpClientPtr httpClient)
    {
        if (httpClient->hasRequestSuccesed())
        {
            m_timerCycle = TIMER_CYCLE;
            NX_LOGX(lm("Statistics report successfully sent to %1")
                .str(httpClient->url()), cl_logINFO);

            const auto now = qnSyncTime->currentDateTime().toUTC();
            m_plannedReportTime = boost::none;

            const int lastNumber = qnGlobalSettings->statisticsReportLastNumber();
            qnGlobalSettings->setStatisticsReportLastNumber(lastNumber + 1);
            qnGlobalSettings->setStatisticsReportLastTime(now);
            qnGlobalSettings->setStatisticsReportLastVersion(QnAppInfo::applicationFullVersion());
            qnGlobalSettings->synchronizeNow();
        }
        else
        {
            if ((m_timerCycle *= 2) > TIMER_CYCLE_MAX)
                m_timerCycle = TIMER_CYCLE_MAX;

            NX_LOGX(lm("doPost to %1 has failed, update timer cycle to %2")
                .str(httpClient->url()).arg(m_timerCycle), cl_logWARNING);
        }

        setupTimer();
    }
}
