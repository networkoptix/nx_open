#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/serialization/json.h>

#include <nx/sdk/common/string.h>

#define NX_PRINT_PREFIX "[axis::DeviceAgent] "
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::axis {

DeviceAgent::DeviceAgent(
    Engine* engine,
    const nx::sdk::DeviceInfo& deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(engine),
    m_parsedManifest(typedManifest),
    m_jsonManifest(QJson::serialized(typedManifest)),
    m_url(deviceInfo.url)
{
    m_auth.setUser(deviceInfo.login);
    m_auth.setPassword(deviceInfo.password);

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;
    for (const auto& eventType: typedManifest.eventTypes)
        deviceAgentManifest.supportedEventTypeIds.push_back(eventType.id);
    NX_PRINT << "Axis DeviceAgent created";
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_PRINT << "Axis DeviceAgent destroyed";
}

void* DeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::analytics::IID_DeviceAgent)
    {
        addRef();
        return static_cast<DeviceAgent*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nx::sdk::Error DeviceAgent::setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

nx::sdk::Error DeviceAgent::setNeededMetadataTypes(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    if (!metadataTypes->eventTypeIds()->count())
    {
        stopFetchingMetadata();
        return nx::sdk::Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

void DeviceAgent::setSettings(const nx::sdk::IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    m_monitor = new Monitor(this, m_url, m_auth, m_handler);
    return m_monitor->startMonitoring(metadataTypes);
}

void DeviceAgent::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
}

const nx::sdk::IString* DeviceAgent::manifest(nx::sdk::Error* error) const
{
    if (m_jsonManifest.isEmpty())
    {
        *error = nx::sdk::Error::unknownError;
        return nullptr;
    }
    return new nx::sdk::common::String(m_jsonManifest);
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

} // nx::vms_server_plugins::analytics::axis
