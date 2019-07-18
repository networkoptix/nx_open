#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/serialization/json.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/helpers/error.h>

#define NX_PRINT_PREFIX "[axis::DeviceAgent] "
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::axis {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(
    Engine* engine,
    const IDeviceInfo* deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(engine),
    m_parsedManifest(typedManifest),
    m_jsonManifest(QJson::serialized(typedManifest)),
    m_url(deviceInfo->url())
{
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());

    NX_PRINT << "Axis DeviceAgent created";
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_PRINT << "Axis DeviceAgent destroyed";
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

Result<void> DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    const auto neededEventTypeIds = toPtr(metadataTypes->eventTypeIds());
    if (!neededEventTypeIds || !neededEventTypeIds->count())
        stopFetchingMetadata();

    return startFetchingMetadata(metadataTypes);
}

nx::sdk::StringMapResult DeviceAgent::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
    return nullptr;
}

SettingsResponseResult DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    m_monitor = new Monitor(this, m_url, m_auth, m_handler.get());
    return m_monitor->startMonitoring(metadataTypes);
}

void DeviceAgent::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
}

StringResult DeviceAgent::manifest() const
{
    if (m_jsonManifest.isEmpty())
        return error(ErrorCode::internalError, "DeviceAgent manifest is empty");

    return new nx::sdk::String(m_jsonManifest);
}

const EventType* DeviceAgent::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_parsedManifest.eventTypes.cbegin(),
        m_parsedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    if (it == m_parsedManifest.eventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

void DeviceAgent::pushPluginDiagnosticEvent(
    IPluginDiagnosticEvent::Level level,
    std::string caption,
    std::string description)
{
    auto diagnosticEvent = makePtr<PluginDiagnosticEvent>(
        level,
        std::move(caption),
        std::move(description));

    m_handler->handlePluginDiagnosticEvent(diagnosticEvent.get());
}

} // nx::vms_server_plugins::analytics::axis
