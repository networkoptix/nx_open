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

#include <nx/sdk/common/string.h>

#include "device_agent.h"
#include "common.h"
#include "attributes_parser.h"
#include "string_helper.h"
#include <nx/utils/log/log_main.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

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

Engine::Engine(nx::sdk::analytics::common::Plugin* plugin): m_plugin(plugin)
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

void* Engine::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Engine)
    {
        addRef();
        return static_cast<Engine*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nx::sdk::analytics::IDeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const QString vendor = QString(deviceInfo->vendor).toLower();

    if (!vendor.startsWith(kHanwhaTechwinVendor) && !vendor.startsWith(kSamsungTechwinVendor))
        return nullptr;

    auto sharedRes = sharedResources(*deviceInfo);
    auto sharedResourceGuard = nx::utils::makeScopeGuard(
        [&sharedRes, deviceInfo, this]()
        {
            if (sharedRes->deviceAgentCount == 0)
                m_sharedResources.remove(QString::fromUtf8(deviceInfo->sharedId));
        });

    auto supportedEventTypeIds = fetchSupportedEventTypeIds(*deviceInfo);
    if (!supportedEventTypeIds)
        return nullptr;

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;
    deviceAgentManifest.supportedEventTypeIds = *supportedEventTypeIds;

    auto deviceAgent = new DeviceAgent(this);
    deviceAgent->setDeviceInfo(*deviceInfo);
    deviceAgent->setDeviceAgentManifest(QJson::serialized(deviceAgentManifest));
    deviceAgent->setEngineManifest(engineManifest());

    ++sharedRes->deviceAgentCount;

    return deviceAgent;
}

const IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return new nx::sdk::common::String(m_manifest);
}

void Engine::setSettings(const nx::sdk::IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

void Engine::executeAction(Action* action, Error* outError)
{
}

boost::optional<QList<QString>> Engine::fetchSupportedEventTypeIds(
    const DeviceInfo& deviceInfo)
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
        cgiParameters, eventStatuses.value, deviceInfo.channel);
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

    const auto& supportedEvents = supportedEventsParameter->possibleValues();
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
                const bool isMatched = fullEventName.startsWith(
                    lm("Channel.%1.%2.").args(channel, altEventName));

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

std::shared_ptr<Engine::SharedResources> Engine::sharedResources(
    const nx::sdk::DeviceInfo& deviceInfo)
{
    const nx::utils::Url url(deviceInfo.url);

    QAuthenticator auth;
    auth.setUser(deviceInfo.login);
    auth.setPassword(deviceInfo.password);

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(deviceInfo.sharedId);
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            deviceInfo.sharedId,
            std::make_shared<SharedResources>(
                deviceInfo.sharedId,
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

nx::sdk::Error Engine::setHandler(nx::sdk::analytics::IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return nx::sdk::Error::noError;
}

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kLibName = "hanwha_analytics_plugin";
static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.hanwha",
    "name": "Hanwha analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::common::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::hanwha::Engine(
                dynamic_cast<nx::sdk::analytics::common::Plugin*>(plugin));
        });
}

} // extern "C"

