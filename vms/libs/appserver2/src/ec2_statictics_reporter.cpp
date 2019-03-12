#include "ec2_statictics_reporter.h"

#include <QtCore/QCollator>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <licensing/license_validator.h>
#include <network/system_helpers.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/random.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

static const std::chrono::hours kDefaultSendCycleTime(30 * 24); //< About a month.
static const std::chrono::hours kSendAfterUpdateTime(3);

static const uint kMinDelayRatio = 30; // 30% is about 9 days.
static const uint kRandomDelayRatio = 50; //< 50% is about 15 days.
static const uint kMaxdelayRation = kMinDelayRatio + kRandomDelayRatio;

static const uint kInitialTimerCycle = 60 * 1000; //< MSecs, update state every minute.
static const uint kGrowTimerCycleRatio = 2; //< Make cycle longer in case of failure.
static const uint kMaxTimerCycle = 24 * 60 * 60 * 1000; //< MSecs, once a day at least.

static const QString kServerReportApi = lit("statserver/api/report");

using namespace nx::vms;

namespace ec2
{
    const QString Ec2StaticticsReporter::DEFAULT_SERVER_API = lit("http://stats.vmsproxy.com");

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

        api::MediaServerDataExList mediaServers;
        errCode = m_ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersExSync(&mediaServers);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& ms : mediaServers) outData->mediaservers.push_back(std::move(ms));

        nx::vms::api::CameraDataExList cameras;
        errCode = m_ec2Connection->getCameraManager(Qn::kSystemAccess)->getCamerasExSync(&cameras);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& cam: cameras)
            outData->cameras.emplace_back(std::move(cam));

        QnLicenseList licenses;
        errCode = m_ec2Connection->getLicenseManager(Qn::kSystemAccess)->getLicensesSync(&licenses);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (const auto& license: licenses)
        {
            nx::vms::api::LicenseData apiLicense;
            fromResourceToApi(license, apiLicense);
            ApiLicenseStatistics statLicense(std::move(apiLicense));
            QnLicenseValidator validator(m_ec2Connection->commonModule());
            statLicense.validation = validator.validationInfo(license);
            outData->licenses.push_back(std::move(statLicense));
        }

        nx::vms::api::EventRuleDataList eventRules;
        errCode = m_ec2Connection->getEventRulesManager(Qn::kSystemAccess)->getEventRulesSync(
            &eventRules);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& rule: eventRules)
            outData->businessRules.emplace_back(std::move(rule));

        errCode = m_ec2Connection->getLayoutManager(Qn::kSystemAccess)->getLayoutsSync(&outData->layouts);
        if (errCode != ErrorCode::ok)
            return errCode;

        nx::vms::api::UserDataList users;
        errCode = m_ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
        if (errCode != ErrorCode::ok)
            return errCode;

        for (auto& u : users) outData->users.push_back(std::move(u));

        errCode = m_ec2Connection->getVideowallManager(Qn::kSystemAccess)->getVideowallsSync(&outData->videowalls);
        if (errCode != ErrorCode::ok)
            return errCode;

        if (outData->systemId.isNull())
            outData->systemId = helpers::currentSystemLocalId(m_ec2Connection->commonModule());

        return ErrorCode::ok;
    }

    ErrorCode Ec2StaticticsReporter::triggerStatisticsReport(
        const ApiStatisticsServerArguments& arguments, ApiStatisticsServerInfo* const outData)
    {
        removeTimer();
        outData->status = lit("initiated");
        outData->systemId = arguments.randomSystemId
            ? QnUuid::createUuid()
            : helpers::currentSystemLocalId(m_ec2Connection->commonModule());

        return initiateReport(&outData->url, &outData->systemId);
    }

    void Ec2StaticticsReporter::setupTimer()
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_timerDisabled)
        {
            m_timerId = nx::utils::TimerManager::instance()->addTimer(
                std::bind(&Ec2StaticticsReporter::timerEvent, this),
                std::chrono::milliseconds(m_timerCycle));

            NX_DEBUG(this, lm("Timer is set with delay %1").arg(m_timerCycle));
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
            NX_DEBUG(this, lm("Automatic report system is disabled"));

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

                NX_DEBUG(this, lm("Last reported version is '%1' while running '%2', plan early report for %3")
                    .arg(reportedVersion).arg(currentVersion)
                    .arg(m_plannedReportTime->toString(Qt::ISODate)));

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

            NX_INFO(this, lm("Last report was at %1, the next planned for %2")
               .arg(lastTime.isValid() ? lastTime.toString(Qt::ISODate) : lit("NEWER"))
               .arg(m_plannedReportTime->toString(Qt::ISODate)));
        }

        return *m_plannedReportTime;
    }

    ErrorCode Ec2StaticticsReporter::initiateReport(QString* reportApi, QnUuid* systemId)
    {
        const auto& settings = m_ec2Connection->commonModule()->globalSettings();
        ApiSystemStatistics data;
        data.reportInfo.number = settings->statisticsReportLastNumber();
        if (systemId)
            data.reportInfo.id = *systemId;

        auto res = collectReportData(nullptr, &data);
        if (res != ErrorCode::ok)
        {
            NX_WARNING(this, lm("Could not collect data, error: %1")
                   .arg(toString(res)));
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

        NX_DEBUG(this, lm("Sending statistics asynchronously to %1")
               .arg(url.toString(QUrl::RemovePassword)));

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
            NX_INFO(this, lm("Statistics report successfully sent to %1")
                .arg(httpClient->url()));

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

            NX_WARNING(this, lm("doPost to %1 has failed, update timer cycle to %2")
                .arg(httpClient->url()).arg(m_timerCycle));
        }

        setupTimer();
    }
}
