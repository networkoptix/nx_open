#include "reporter.h"

#include <future>

#include <QtCore/QCollator>

#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <licensing/license_validator.h>
#include <network/system_helpers.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/statistics/settings.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/app_info.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>

#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/plugin_descriptor_manager.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include <nx/vms/server/statistics/data_conversion_utils.h>

static const std::chrono::hours kDefaultSendCycleTime(30 * 24); //< About a month.
static const std::chrono::hours kSendAfterUpdateTime(3);

static const uint kMinDelayRatio = 30; // 30% is about 9 days.
static const uint kRandomDelayRatio = 50; //< 50% is about 15 days.
static const uint kMaxDelayRatio = kMinDelayRatio + kRandomDelayRatio;

static const std::chrono::seconds kInitialTimerCycle(10);
static const std::chrono::milliseconds::rep kGrowTimerCycleRatio = 2; //< Make cycle longer in case of failure.
static const std::chrono::hours kMaxTimerCycle(24);

static const QString kServerReportApi = lit("statserver/api/report");

using namespace nx::vms::api;

namespace nx::vms::server::statistics {

Reporter::Reporter(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_timerCycle(kInitialTimerCycle),
    m_timerManager(commonModule->timerManager())
{
    NX_CRITICAL(kMaxDelayRatio <= 100);
    setupTimer();
}

Reporter::~Reporter()
{
    removeTimer();
}

ec2::ErrorCode Reporter::collectReportData(SystemStatistics* const outData)
{
    const ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    if(!ec2Connection)
        return ec2::ErrorCode::ioError;

    ec2::ErrorCode errCode;

    api::MediaServerDataExList mediaServers;
    errCode = ec2Connection
        ->getMediaServerManager(Qn::kSystemAccess)
        ->getServersExSync(&mediaServers);

    if (errCode != ec2::ErrorCode::ok)
        return errCode;

    nx::utils::promise<ExtendedPluginInfoByServer> promise;
    rest::Handle requestHandle = -1;
    requestHandle = commonModule()
        ->currentServer()
        ->restConnection()
        ->getExtendedPluginInformation(
            [&promise, &requestHandle](
                bool success,
                rest::Handle requestId,
                const ExtendedPluginInfoByServer& result) mutable
            {
                if (success && requestHandle == requestId)
                    promise.set_value(result);
                else
                    promise.set_value({});
            });

    auto future = promise.get_future();
    future.wait();

    nx::vms::api::ExtendedPluginInfoByServer pluginInfoByServer = future.get();
    for (auto& mediaServer: mediaServers)
    {
        StatisticsMediaServerData mediaServerStatistics = toStatisticsData(std::move(mediaServer));
        if (auto it = pluginInfoByServer.find(mediaServerStatistics.id);
            it != pluginInfoByServer.cend())
        {
            for (PluginInfoEx& pluginInfo: it->second)
            {
                mediaServerStatistics.pluginInfo.push_back(
                    toStatisticsData(std::move(pluginInfo)));
            }
        }
        outData->mediaservers.push_back(std::move(mediaServerStatistics));
    }

    nx::vms::api::CameraDataExList cameras;
    errCode = ec2Connection->getCameraManager(Qn::kSystemAccess)->getCamerasExSync(&cameras);
    if (errCode != ec2::ErrorCode::ok)
        return errCode;

    for (auto& camInfo: cameras)
        outData->cameras.emplace_back(fullDeviceStatistics(std::move(camInfo)));

    QnLicenseList licenses;
    errCode = ec2Connection->getLicenseManager(Qn::kSystemAccess)->getLicensesSync(&licenses);
    if (errCode != ec2::ErrorCode::ok)
        return errCode;

    for (const auto& license: licenses)
    {
        nx::vms::api::LicenseData apiLicense;
        ec2::fromResourceToApi(license, apiLicense);
        StatisticsLicenseData statLicense = toStatisticsData(std::move(apiLicense));
        QnLicenseValidator validator(ec2Connection->commonModule());
        statLicense.validation = validator.validationInfo(license);
        outData->licenses.push_back(std::move(statLicense));
    }

    nx::vms::api::EventRuleDataList eventRules;
    errCode = ec2Connection->getEventRulesManager(Qn::kSystemAccess)->getEventRulesSync(
        &eventRules);
    if (errCode != ec2::ErrorCode::ok)
        return errCode;

    for (auto& rule: eventRules)
        outData->businessRules.emplace_back(toStatisticsData(std::move(rule)));

    nx::vms::api::UserDataList users;
    errCode = ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
    if (errCode != ec2::ErrorCode::ok)
        return errCode;

    for (auto& u: users)
        outData->users.push_back(toStatisticsData(std::move(u)));

    if (outData->systemId.isNull())
        outData->systemId = helpers::currentSystemLocalId(ec2Connection->commonModule());

    return ec2::ErrorCode::ok;
}

ec2::ErrorCode Reporter::triggerStatisticsReport(
    const StatisticsServerArguments& arguments, StatisticsServerInfo* const outData)
{
    removeTimer();
    outData->status = lit("initiated");
    outData->systemId = arguments.randomSystemId
        ? QnUuid::createUuid()
        : helpers::currentSystemLocalId(commonModule());

    return initiateReport(&outData->url, &outData->systemId);
}

void Reporter::setupTimer()
{
    QnMutexLocker lk(&m_mutex);
    if (!m_timerDisabled)
    {
        m_timerId = m_timerManager->addTimer([this](auto) { timerEvent(); }, m_timerCycle);
        NX_VERBOSE(this, "Timer is set with delay %1", m_timerCycle);
    }
}

void Reporter::removeTimer()
{
    decltype(m_timerId) timerId;
    {
        QnMutexLocker lk(&m_mutex);
        m_timerDisabled = true;
        m_timerId.swap(timerId);
    }

    if (timerId)
        m_timerManager->joinAndDeleteTimer(*timerId);

    if (auto client = m_httpClient)
        client->pleaseStopSync();

    {
        QnMutexLocker lk(&m_mutex);
        m_timerDisabled = false;
    }
}

void Reporter::timerEvent()
{
    const auto& settings = commonModule()->globalSettings();
    if (!settings->isInitialized())
    {
        NX_VERBOSE(this, "Settings are not initialized yet");

        // Try again later.
        setupTimer();
        return;
    }

    if (!settings->isStatisticsAllowed() || settings->isNewSystem())
    {
        NX_DEBUG(this, "Automatic report system is disabled");

        // Better luck next time (if report system will be enabled by another mediaserver)
        setupTimer();
        return;
    }

    const QDateTime now = qnSyncTime->currentDateTime().toUTC();
    if (const auto plannedTime = plannedReportTime(now); plannedTime <= now)
    {
        if (initiateReport() == ec2::ErrorCode::ok)
            return;
    }
    else
    {
        NX_VERBOSE(this, "Planned time %1 has not come yet", plannedTime.toString(Qt::ISODate));
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

QDateTime Reporter::plannedReportTime(const QDateTime& now)
{
    const auto& settings = commonModule()->globalSettings();
    if (m_firstTime)
    {
        m_firstTime = false;
        const auto reportedVersion = settings->statisticsReportLastVersion();
        const auto currentVersion = nx::utils::AppInfo::applicationFullVersion();

        QCollator collator;
        collator.setNumericMode(true);
        if (collator.compare(currentVersion, reportedVersion) > 0)
        {
            const auto timeCycle = nx::utils::parseTimerDuration(
                settings->statisticsReportUpdateDelay(), kSendAfterUpdateTime);
            NX_VERBOSE(this, "Base update delay %1", timeCycle);

            m_plannedReportTime = now.addSecs(nx::utils::random::number(
                  convertToSeconds(timeCycle) * kMinDelayRatio / 100,
                  convertToSeconds(timeCycle) * kMaxDelayRatio / 100));
            NX_DEBUG(this, "Last reported version is '%1' while running '%2', plan early report for %3",
                reportedVersion, currentVersion, m_plannedReportTime->toString(Qt::ISODate));

            return *m_plannedReportTime;
        }
    }

    const auto timeCycle = nx::utils::parseTimerDuration(
        settings->statisticsReportTimeCycle(), kDefaultSendCycleTime);
    if (!m_plannedReportTime || *m_plannedReportTime > now.addSecs(convertToSeconds(timeCycle)))
    {
        NX_VERBOSE(this, "Base cycle delay %1", timeCycle);

        const QDateTime lastTime = settings->statisticsReportLastTime();
        m_plannedReportTime = (lastTime.isValid() ? lastTime : now).addSecs(
            nx::utils::random::number<uint>(
                convertToSeconds(timeCycle) * kMinDelayRatio / 100,
                convertToSeconds(timeCycle) * kMaxDelayRatio / 100));

        NX_INFO(this, lm("Last report was at %1, the next planned for %2")
           .arg(lastTime.isValid() ? lastTime.toString(Qt::ISODate) : lit("NEWER"))
           .arg(m_plannedReportTime->toString(Qt::ISODate)));
    }

    return *m_plannedReportTime;
}

ec2::ErrorCode Reporter::initiateReport(QString* reportApi, QnUuid* systemId)
{
    const auto& settings = commonModule()->globalSettings();
    SystemStatistics data;
    data.reportInfo.number = settings->statisticsReportLastNumber();
    if (systemId)
        data.reportInfo.id = *systemId;

    auto res = collectReportData(&data);
    if (res != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, lm("Could not collect data, error: %1").arg(toString(res)));
        return res;
    }

    m_httpClient = nx::network::http::AsyncHttpClient::create();
    connect(m_httpClient.get(), &nx::network::http::AsyncHttpClient::done,
            this, &Reporter::finishReport, Qt::DirectConnection);

    m_httpClient->setUserName(nx::vms::statistics::kDefaultUser);
    m_httpClient->setUserPassword(nx::vms::statistics::kDefaultPassword);

    const QString configApi = settings->statisticsReportServerApi();
    const QString serverApi = configApi.isEmpty()
        ? nx::vms::statistics::kDefaultStatisticsServer
        : configApi;

    const nx::utils::Url url = lit("%1/%2").arg(serverApi).arg(kServerReportApi);
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    auto content = QJson::serialized(data);
    m_httpClient->addAdditionalHeader("Content-MD5", nx::utils::md5(content).toHex());
    m_httpClient->doPost(url, contentType, std::move(content));

    NX_DEBUG(this, lm("Sending statistics asynchronously to %1")
           .arg(url.toString(QUrl::RemovePassword)));

    if (reportApi)
        *reportApi = url.toString();

    return ec2::ErrorCode::ok;
}

void Reporter::finishReport(nx::network::http::AsyncHttpClientPtr httpClient)
{
    const auto& settings = commonModule()->globalSettings();

    if (httpClient->hasRequestSucceeded())
    {
        m_timerCycle = kInitialTimerCycle;
        NX_INFO(this, "Statistics report successfully sent to %1", httpClient->url());

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
        if ((m_timerCycle *= kGrowTimerCycleRatio) > kMaxTimerCycle)
            m_timerCycle = kMaxTimerCycle;

        NX_WARNING(this, "POST to %1 has failed (%2), update timer cycle to %3",
            httpClient->url(),
            httpClient->response()
                ? httpClient->response()->statusLine.toString().trimmed()
                : SystemError::toString(httpClient->lastSysErrorCode()),
            m_timerCycle);
    }

    setupTimer();
}

namespace {

template<typename Descriptor>
std::vector<DeviceAnalyticsTypeInfo> deviceAnalyticsTypeInfo(
    QnCommonModule* commonModule,
    const QnVirtualCameraResourcePtr device,
    const std::map<QnUuid, std::set<QString>>& supportedItemsByEngineId,
    std::function<std::optional<Descriptor>(const QString& itemId)> descriptorRetriever)
{
    if (!NX_ASSERT(device, "device is null"))
        return {};

    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto pluginDescriptorManager = commonModule->analyticsPluginDescriptorManager();

    auto providerFromItemDescriptor =
        [](const auto& itemDescriptor,
            const QnUuid& engineId,
            const QString& pluginName)
        {
            for (const auto& scope: itemDescriptor.scopes)
            {
                if (scope.engineId == engineId && !scope.provider.isEmpty())
                    return scope.provider;
            }
            return pluginName;
        };

    std::vector<DeviceAnalyticsTypeInfo> result;
    for (const auto& [engineId, supportedItemIds]: supportedItemsByEngineId)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        if (!engineDescriptor)
        {
            NX_WARNING(typeid(Reporter),
                "Unable to find an Engine descriptor for the Engine %1; Device %2",
                engineId, device);
            continue;
        }

        if (!engineDescriptor->capabilities.testFlag(
            nx::vms::api::analytics::EngineManifest::Capability::deviceDependent))
        {
            NX_DEBUG(typeid(Reporter),
                "Ignoring device independent Engine %1 (%2); Device: %3",
                engineDescriptor->name, engineDescriptor->id, device);
            continue;
        }

        const auto pluginDescriptor = pluginDescriptorManager->descriptor(
            engineDescriptor->pluginId);

        if (!pluginDescriptor)
        {
            NX_WARNING(typeid(Reporter),
                "Unable to find a Plugin descriptor for the plugin %1",
                engineDescriptor->pluginId);
            continue;
        }

        for (const auto& itemId: supportedItemIds)
        {
            const auto descriptor = descriptorRetriever(itemId);
            if (!descriptor)
            {
                NX_WARNING(typeid(Reporter),
                    "Unable to find a descriptor for an item %1", itemId);
                continue;
            }

            DeviceAnalyticsTypeInfo itemInfo;
            itemInfo.id = itemId;
            itemInfo.name = descriptor->name;
            itemInfo.provider = providerFromItemDescriptor(
                *descriptor,
                engineDescriptor->id,
                pluginDescriptor->name);

            result.push_back(itemInfo);
        }
    }

    return result;
}

} // namespace

StatisticsCameraData Reporter::fullDeviceStatistics(CameraDataEx&& deviceInfo)
{
    using namespace nx::vms::api::analytics;

    StatisticsCameraData result = toStatisticsData(std::move(deviceInfo));

    const auto commonModule = this->commonModule();
    if (!NX_ASSERT(commonModule, "Unable to access common module"))
        return result;

    const auto resourcePool = commonModule->resourcePool();
    if (!NX_ASSERT(resourcePool, "Unable to access resource pool"))
        return result;

    auto device = resourcePool->getResourceById<QnVirtualCameraResource>(deviceInfo.id);
    if (!device)
        return result;

    result.analyticsInfo.supportedEventTypes = deviceAnalyticsTypeInfo<EventTypeDescriptor>(
        commonModule,
        device,
        device->supportedEventTypes(),
        [commonModule](const QString& eventTypeId)
        {
            return commonModule->analyticsEventTypeDescriptorManager()->descriptor(eventTypeId);
        });

    result.analyticsInfo.supportedObjectTypes = deviceAnalyticsTypeInfo<ObjectTypeDescriptor>(
        commonModule,
        device,
        device->supportedObjectTypes(),
        [commonModule](const QString& objectTypeId)
        {
            return commonModule->analyticsObjectTypeDescriptorManager()->descriptor(objectTypeId);
        });

    return result;
}

} // namespace nx::vms::server::statistics
