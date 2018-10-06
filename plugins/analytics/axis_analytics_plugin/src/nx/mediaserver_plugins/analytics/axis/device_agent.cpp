#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_metadata_packet.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/serialization/json.h>

#define NX_PRINT_PREFIX "[axis::DeviceAgent] "
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

DeviceAgent::DeviceAgent(
    Engine* engine,
    const nx::sdk::DeviceInfo& deviceInfo,
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
    for (const auto& eventType: typedManifest.outputEventTypes)
        deviceAgentManifest.supportedEventTypes.push_back(eventType.id);
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

nx::sdk::Error DeviceAgent::setMetadataHandler(
    nx::sdk::analytics::MetadataHandler* metadataHandler)
{
    m_metadataHandler = metadataHandler;
    return nx::sdk::Error::noError;
}

void DeviceAgent::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const char* const* typeList, int typeListSize)
{
    m_monitor = new Monitor(this, m_url, m_auth, m_metadataHandler);
    return m_monitor->startMonitoring(typeList, typeListSize);
}

nx::sdk::Error DeviceAgent::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
    return nx::sdk::Error::noError;
}

const char* DeviceAgent::manifest(nx::sdk::Error* error)
{
    if (m_manifest.isEmpty())
    {
        *error = nx::sdk::Error::unknownError;
        return nullptr;
    }
    *error = nx::sdk::Error::noError;
    m_givenManifests.push_back(m_manifest);
    return m_manifest.constData();
}

void DeviceAgent::freeManifest(const char* data)
{
    // Do nothing. Memory allocated for Manifests is stored in m_givenManifests list and will be
    // released in DeviceAgent's destructor.
}

const EventType* DeviceAgent::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    if (it == m_typedManifest.outputEventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
