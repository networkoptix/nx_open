#include "engine.h"

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/scope_guard.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "device_agent.h"
#include "common.h"
#include "attributes_parser.h"
#include "string_helper.h"
#include <nx/utils/log/log_main.h>

namespace nx::vms_server_plugins::analytics::hanwha {

namespace {

static const QString kSamsungTechwinVendor("samsung");
static const QString kHanwhaTechwinVendor("hanwha");

static const QString kVideoAnalytics("VideoAnalytics");
static const QString kAudioAnalytics("AudioAnalytics");
static const QString kQueueEvent("QueueEvent");
static const std::map<QString, QString> kSpecialEventTypes = {
    {kVideoAnalytics, kVideoAnalytics},
    {kAudioAnalytics, kAudioAnalytics},
    {kQueueEvent, "Queue"}
};

// TODO: Decide on these unused constants.
static const std::chrono::milliseconds kAttributesTimeout(4000);
static const QString kAttributesPath("/stw-cgi/attributes.cgi/cgis");

QString specialEventName(const QString& eventName)
{
    const auto itr = kSpecialEventTypes.find(eventName);
    if (itr == kSpecialEventTypes.cend())
        return QString();

    return itr->second;
}

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::EngineManifest& engineManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<MetadataMonitor>(engineManifest, url, auth)),
    sharedContext(std::make_shared<vms::server::plugins::HanwhaSharedResourceContext>(
        sharedId))
{
    sharedContext->setResourceAccess(url, auth);
}

void Engine::SharedResources::setResourceAccess(
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    sharedContext->setResourceAccess(url, auth);
    monitor->setResourceAccess(url, auth);
}

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    QFile f(":/hanwha/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    {
        QFile file("plugins/hanwha/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            m_manifest = file.readAll();
        }
    }
    m_engineManifest = QJson::deserialized<Hanwha::EngineManifest>(m_manifest);
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (!isCompatible(deviceInfo))
    {
        *outResult = error(ErrorCode::otherError, "Device is not compatible with the Engine");
        return;
    }

    auto sharedRes = sharedResources(deviceInfo);
    auto sharedResourceGuard = nx::utils::makeScopeGuard(
        [&sharedRes, deviceInfo, this]()
        {
            if (sharedRes->deviceAgentCount == 0)
                m_sharedResources.remove(QString::fromUtf8(deviceInfo->sharedId()));
        });

    auto supportedEventTypeIds = fetchSupportedEventTypeIds(deviceInfo);
    if (!supportedEventTypeIds)
    {
        NX_DEBUG(this, "Supported Event Type list is empty for the Device %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return;
    }

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;
    deviceAgentManifest.supportedEventTypeIds = *supportedEventTypeIds;

    // DeviceAgent should understand all engine's object types.
    deviceAgentManifest.supportedObjectTypeIds.reserve(m_engineManifest.objectTypes.size());
    for (const nx::vms::api::analytics::ObjectType& objectType: m_engineManifest.objectTypes)
        deviceAgentManifest.supportedObjectTypeIds.push_back(objectType.id);

    const auto deviceAgent = new DeviceAgent(this, deviceInfo);
    deviceAgent->readCameraSettings();
    deviceAgent->setDeviceAgentManifest(QJson::serialized(deviceAgentManifest));

    ++sharedRes->deviceAgentCount;

    *outResult = deviceAgent;
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(m_manifest);
}

void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

void Engine::doSetSettings(
    Result<const IStringMap*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void Engine::doExecuteAction(Result<IAction::Result>* /*outResult*/, const IAction* /*action*/)
{
}

boost::optional<QList<QString>> Engine::fetchSupportedEventTypeIds(
    const IDeviceInfo* deviceInfo)
{
    using namespace nx::vms::server::plugins;

    auto sharedRes = sharedResources(deviceInfo);

    const auto& information = sharedRes->sharedContext->information();
    if (!information)
        return boost::none;
    const auto& cgiParameters = information->cgiParameters;
    if (!cgiParameters.isValid())
        return boost::none;

    const auto& eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
        return boost::none;

    return eventTypeIdsFromParameters(
        sharedRes->sharedContext->url(),
        cgiParameters, eventStatuses.value, deviceInfo->channelNumber());
}

boost::optional<QList<QString>> Engine::eventTypeIdsFromParameters(
    const nx::utils::Url& url,
    const nx::vms::server::plugins::HanwhaCgiParameters& parameters,
    const nx::vms::server::plugins::HanwhaResponse& eventStatuses,
    int channel) const
{
    if (!parameters.isValid())
        return boost::none;

    auto supportedEventsParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventsParameter.is_initialized())
        return boost::none;

    QSet<QString> result;

    auto supportedEvents = supportedEventsParameter->possibleValues();
    const auto alarmInputParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/AlarmInput");

    if (alarmInputParameter)
        supportedEvents.push_back(alarmInputParameter->name());

    NX_VERBOSE(this, lm("camera %1 report supported analytics events %2").arg(url).arg(supportedEvents));
    for (const auto& eventName: supportedEvents)
    {
        auto eventTypeId = m_engineManifest.eventTypeIdByName(eventName);
        if (!eventTypeId.isEmpty())
            result.insert(eventTypeId);

        const auto altEventName = specialEventName(eventName);
        if (!altEventName.isEmpty())
        {
            const auto& responseParameters = eventStatuses.response();
            for (const auto& entry: responseParameters)
            {
                const auto& fullEventName = entry.first;
                const bool isMatched =
                    fullEventName.startsWith(lm("Channel.%1.%2.").args(channel, altEventName));

                if (isMatched)
                {
                    eventTypeId = m_engineManifest.eventTypeIdByName(fullEventName);
                    if (!eventTypeId.isNull())
                        result.insert(eventTypeId);
                }
            }
        }
    }

    return QList<QString>::fromSet(result);
}

const Hanwha::EngineManifest& Engine::engineManifest() const
{
    return m_engineManifest;
}

MetadataMonitor* Engine::monitor(
    const QString& sharedId,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    std::shared_ptr<SharedResources> monitorCounter;
    {
        QnMutexLocker lock(&m_mutex);
        auto sharedResourcesItr = m_sharedResources.find(sharedId);
        if (sharedResourcesItr == m_sharedResources.cend())
        {
            sharedResourcesItr = m_sharedResources.insert(
                sharedId,
                std::make_shared<SharedResources>(
                    sharedId,
                    engineManifest(),
                    url,
                    auth));
        }

        monitorCounter = sharedResourcesItr.value();
        ++monitorCounter->monitorUsageCount;
    }

    return monitorCounter->monitor.get();
}

void Engine::deviceAgentStoppedToUseMonitor(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    if (sharedResources->monitorUsageCount)
        --sharedResources->monitorUsageCount;

    if (sharedResources->monitorUsageCount <= 0)
        m_sharedResources[sharedId]->monitor->stopMonitoring();
}

void Engine::deviceAgentIsAboutToBeDestroyed(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    --sharedResources->deviceAgentCount;
    NX_ASSERT(sharedResources->deviceAgentCount >= 0);
    if (sharedResources->deviceAgentCount <= 0)
        m_sharedResources.remove(sharedId);
}

std::shared_ptr<Engine::SharedResources> Engine::sharedResources(const IDeviceInfo* deviceInfo)
{
    const nx::utils::Url url(deviceInfo->url());

    QAuthenticator auth;
    auth.setUser(deviceInfo->login());
    auth.setPassword(deviceInfo->password());

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(deviceInfo->sharedId());
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            deviceInfo->sharedId(),
            std::make_shared<SharedResources>(
                deviceInfo->sharedId(),
                engineManifest(),
                url,
                auth));
    }
    else
    {
        sharedResourcesItr.value()->setResourceAccess(url, auth);
    }

    return sharedResourcesItr.value();
}

void Engine::setHandler(IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const QString vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kHanwhaTechwinVendor) || vendor.startsWith(kSamsungTechwinVendor);
}

std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> Engine::sharedContext(
    QString m_sharedId)
{
    auto sharedResourcesItr = m_sharedResources.find(m_sharedId);
    if (sharedResourcesItr != m_sharedResources.cend())
    {
        return sharedResourcesItr.value()->sharedContext;
    }
    else
        return {};
}

} // namespace nx::vms_server_plugins::analytics::hanwha

namespace {

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.hanwha",
    "name": "Hanwha analytics plugin",
    "description": "Supports built-in analytics on Hanwha cameras",
    "version": "1.0.0",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::hanwha::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
