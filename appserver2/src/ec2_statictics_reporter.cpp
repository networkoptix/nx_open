#include "ec2_statictics_reporter.h"

#include <QtCore/QCollator>

#include <api/global_settings.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/random.h>
#include <nx/utils/app_info.h>

#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <network/system_helpers.h>

#include "ec2_connection.h"
#include <licensing/license_validator.h>

static const std::chrono::hours kDefaultSendCycleTime(30 * 24); //< About a month.
static const std::chrono::hours kSendAfterUpdateTime(3);

static const uint kMinDelayRatio = 30; // 30% is about 9 days.
static const uint kRandomDelayRatio = 50; //< 50% is about 15 days.
static const uint kMaxdelayRation = kMinDelayRatio + kRandomDelayRatio;

static const uint kInitialTimerCycle = 60 * 1000; //< MSecs, update state every minute.
static const uint kGrowTimerCycleRatio = 2; //< Make cycle longer in case of failure.
static const uint kMaxTimerCycle = 24 * 60 * 60 * 1000; //< MSecs, once a day at least.

static const QString kServerReportApi = lit("statserver/api/report");

namespace ec2
{
    const QString Ec2StaticticsReporter::DEFAULT_SERVER_API = lit("http://stats.networkoptix.com");

    // Hardcoded credentials (because of no way to keep it better)
    const QString Ec2StaticticsReporter::AUTH_USER = lit("nx");
    const QString Ec2StaticticsReporter::AUTH_PASSWORD = lit(
                "f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");

    Ec2StaticticsReporter::Ec2StaticticsReporter(ec2::AbstractECConnection* ec2Connection):
        m_ec2Connection(ec2Connection),
        m_firstTime(true),
        m_timerCycle(kInitialTimerCycle),
        m_timerDisabled(false),
        m_timerId(boost::none)
    {
        NX_CRITICAL(kMaxdelayRation <= 100);
        setupTimer();
    }

    Ec2StaticticsReporter::~Ec2StaticticsReporter()
    {
        removeTimer();
    }

    ErrorCode Ec2StaticticsReporter::collectReportData(std::nullptr_t, ApiSystemStatistics* const outData)
    {
        if(!m_ec2Connection)
            return ErrorCode::ioError;

        ErrorCode errCode;

        ApiMediaServerDataExList mediaServers;
        errCode = m_ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersExSync(&mediaServers);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& ms : mediaServers) outData->mediaservers.push_back(std::move(ms));


        ApiCameraDataExList cameras;
        errCode = m_ec2Connection->getCameraManager(Qn::kSystemAccess)->getCamerasExSync(&cameras);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (ApiCameraDataEx& cam: cameras)
            if (cam.typeId != QnResourceTypePool::kDesktopCameraTypeUuid)
                outData->cameras.push_back(std::move(cam));

        QnLicenseList licenses;
        errCode = m_ec2Connection->getLicenseManager(Qn::kSystemAccess)->getLicensesSync(&licenses);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (const auto& license: licenses)
        {
            ApiLicenseData apiLicense;
            fromResourceToApi(license, apiLicense);
            ApiLicenseStatistics statLicense(std::move(apiLicense));
            QnLicenseValidator validator(m_ec2Connection->commonModule());
            statLicense.validation = validator.validationInfo(license);
            outData->licenses.push_back(std::move(statLicense));
        }

        nx::vms::event::RuleList bRules;
        errCode = m_ec2Connection->getBusinessEventManager(Qn::kSystemAccess)->getBusinessRulesSync(&bRules);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& br: bRules)
        {
            ApiBusinessRuleData apiData;
            fromResourceToApi(br, apiData);
            outData->businessRules.push_back(std::move(apiData));
        }

        errCode = m_ec2Connection->getLayoutManager(Qn::kSystemAccess)->getLayoutsSync(&outData->layouts);
        if (errCode != ErrorCode::ok)
            return errCode;

        ApiUserDataList users;
        errCode = m_ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& u : users) outData->users.push_back(std::move(u));

        #undef dbManager_queryOrReturn
        #undef dbManager_queryOrReturn_uuid

        outData->systemId = helpers::currentSystemLocalId(m_ec2Connection->commonModule());
        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData)
    {
        removeTimer();
        outData->systemId = helpers::currentSystemLocalId(m_ec2Connection->commonModule());
        outData->status = lit("initiated");
        return initiateReport(&outData->url);
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
        decltype(m_timerId) timerId;
        {
            QnMutexLocker lk(&m_mutex);
            m_timerDisabled = true;
            m_timerId.swap(timerId);
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
        const auto& settings = m_ec2Connection->commonModule()->globalSettings();
        if (!settings->isInitialized())
        {
            /* Try again. */
            setupTimer();
            return;
        }

        if (!settings->isStatisticsAllowed()
            || settings->isNewSystem())
        {
            NX_LOGX(lm("Automatic report system is disabled"), cl_logDEBUG1);

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

    template<typename Rep, typename Period>
    uint convertToSeconds(const std::chrono::duration<Rep, Period>& duration)
    {
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        return static_cast<uint>(seconds.count());
    }

    QDateTime Ec2StaticticsReporter::plannedReportTime(const QDateTime& now)
    {
        const auto& settings = m_ec2Connection->commonModule()->globalSettings();
        if (m_firstTime)
        {
            m_firstTime = false;
            const auto reportedVersion = settings->statisticsReportLastVersion();
            const auto currentVersion = nx::utils::AppInfo::applicationFullVersion();

            QCollator collator;
            collator.setNumericMode(true);
            if (collator.compare(currentVersion, reportedVersion) > 0)
            {
                const uint timeCycle = convertToSeconds(nx::utils::parseTimerDuration(
                    settings->statisticsReportTimeCycle(), kSendAfterUpdateTime));

                m_plannedReportTime = now.addSecs(nx::utils::random::number(
                      timeCycle * kMinDelayRatio / 100,
                      timeCycle * kMaxdelayRation / 100));

                NX_LOGX(lm("Last reported version is '%1' while running '%2', plan early report for %3")
                    .arg(reportedVersion).arg(currentVersion)
                    .arg(m_plannedReportTime->toString(Qt::ISODate)), cl_logDEBUG1);

                return *m_plannedReportTime;
            }
        }

        const uint timeCycle = convertToSeconds(nx::utils::parseTimerDuration(
            settings->statisticsReportTimeCycle(), kDefaultSendCycleTime));

        const uint minDelay = timeCycle * kMinDelayRatio / 100;
        const uint maxDelay = timeCycle * kMaxdelayRation / 100;
        if (!m_plannedReportTime || *m_plannedReportTime > now.addSecs(maxDelay))
        {
            const QDateTime lastTime = settings->statisticsReportLastTime();
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
        const auto& settings = m_ec2Connection->commonModule()->globalSettings();
        ApiSystemStatistics data;
        data.reportInfo.number = settings->statisticsReportLastNumber();
        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_LOGX(lm("Could not collect data, error: %1")
                   .arg(toString(res)), cl_logWARNING);
            return res;
        }

        m_httpClient = nx::network::http::AsyncHttpClient::create();
        connect(m_httpClient.get(), &nx::network::http::AsyncHttpClient::done,
                this, &Ec2StaticticsReporter::finishReport, Qt::DirectConnection);

        m_httpClient->setUserName(AUTH_USER);
        m_httpClient->setUserPassword(AUTH_PASSWORD);

        const QString configApi = settings->statisticsReportServerApi();
        const QString serverApi = configApi.isEmpty() ? DEFAULT_SERVER_API : configApi;
        const nx::utils::Url url = lit("%1/%2").arg(serverApi).arg(kServerReportApi);
        const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        m_httpClient->doPost(url, contentType, QJson::serialized(data));

        NX_LOGX(lm("Sending statistics asynchronously to %1")
               .arg(url.toString(QUrl::RemovePassword)), cl_logDEBUG1);

        if (reportApi)
            *reportApi = url.toString();

        return ErrorCode::ok;
    }

    void Ec2StaticticsReporter::finishReport(nx::network::http::AsyncHttpClientPtr httpClient)
    {
        const auto& settings = m_ec2Connection->commonModule()->globalSettings();

        if (httpClient->hasRequestSucceeded())
        {
            m_timerCycle = kInitialTimerCycle;
            NX_LOGX(lm("Statistics report successfully sent to %1")
                .arg(httpClient->url()), cl_logINFO);

            const auto now = qnSyncTime->currentDateTime().toUTC();
            m_plannedReportTime = boost::none;

            const int lastNumber = settings->statisticsReportLastNumber();
            settings->setStatisticsReportLastNumber(lastNumber + 1);
            settings->setStatisticsReportLastTime(now);
            settings->setStatisticsReportLastVersion(nx::utils::AppInfo::applicationFullVersion());
            settings->synchronizeNow();
        }
        else
        {
            if ((m_timerCycle *= 2) > kMaxTimerCycle)
                m_timerCycle = kMaxTimerCycle;

            NX_LOGX(lm("doPost to %1 has failed, update timer cycle to %2")
                .arg(httpClient->url()).arg(m_timerCycle), cl_logWARNING);
        }

        setupTimer();
    }
}
