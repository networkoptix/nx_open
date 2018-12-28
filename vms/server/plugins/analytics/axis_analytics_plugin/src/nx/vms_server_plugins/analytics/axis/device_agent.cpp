#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/serialization/json.h>

#include <nx/sdk/helpers/string.h>

#define NX_PRINT_PREFIX "[axis::DeviceAgent] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace axis {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(
    Engine* engine,
    const DeviceInfo& deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(engine),
    m_typedManifest(typedManifest),
    m_manifest(QJson::serialized(typedManifest)),
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
    if (interfaceId == IID_DeviceAgent)
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

Error DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error DeviceAgent::setNeededMetadataTypes(
    const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->eventTypeIds()->count())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

void DeviceAgent::setSettings(const IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

Error DeviceAgent::startFetchingMetadata(
    const IMetadataTypes* metadataTypes)
{
    m_monitor = new Monitor(this, m_url, m_auth, m_handler);
    return m_monitor->startMonitoring(metadataTypes);
}

void DeviceAgent::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
}

const IString* DeviceAgent::manifest(Error* error) const
{
    if (m_manifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }
    *error = Error::noError;
    m_givenManifests.push_back(m_manifest);
    return new nx::sdk::String(m_manifest);
}

const EventType* DeviceAgent::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.eventTypes.cbegin(),
        m_typedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    if (it == m_typedManifest.eventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

} // namespace axis
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
