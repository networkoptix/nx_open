#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
//#include <nx/api/analytics/driver_manifest.h>

#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

#include "nx/api/vca_driver_manifest.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

Manager::Manager(
    const nx::sdk::ResourceInfo& resourceInfo, const QByteArray& pluginManifest)
{
    m_url = resourceInfo.url;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);

    m_typedPluginManifest = QJson::deserialized<nx::api::VcaAnalyticsDriverManifest>(pluginManifest);

    nx::api::AnalyticsDeviceManifest typedCameraManifest;
    for (const auto& eventType: m_typedPluginManifest.outputEventTypes)
        typedCameraManifest.supportedEventTypes.push_back(eventType.eventTypeId);

    // All vca managers support all plugin events.
    m_cameraManifest = QJson::serialized(typedCameraManifest);


    NX_PRINT << "Ctor :" << this;
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_PRINT << "Dtor :" << this;
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_MetadataManager)
    {
        addRef();
        return static_cast<AbstractMetadataManager*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nx::sdk::Error Manager::startFetchingMetadata(nx::sdk::metadata::AbstractMetadataHandler* handler,
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    m_monitor = new Monitor(this, m_url, m_auth, handler);
    return m_monitor->startMonitoring(eventTypeList, eventTypeListSize);
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
{
    return m_cameraManifest;
}

void Manager::freeManifest(const char* data)
{
    // Do nothing. Memory allocated for Manifests is stored in m_givenManifests list and will be
    // released in Manager's destructor.
}

const nx::api::VcaAnalyticsEventType& Manager::getVcaAnalyticsEventTypeByEventTypeId(
    const QnUuid& id) const
{
    //there are only few elements, so linear search is the fastest
    const auto it = std::find_if(
        m_typedPluginManifest.outputEventTypes.cbegin(),
        m_typedPluginManifest.outputEventTypes.cend(),
        [&id](const nx::api::VcaAnalyticsEventType& event){ return event.eventTypeId == id; });

    return
        (it != m_typedPluginManifest.outputEventTypes.cend())
        ? *it
        : m_emptyEvent;
}
const nx::api::VcaAnalyticsEventType& Manager::getVcaAnalyticsEventTypeByInternalName(
    const char* name) const
{
    //there are only few elements, so linear search is the fastest
    QString qName(QString::fromUtf8(name));
    const auto it = std::find_if(
        m_typedPluginManifest.outputEventTypes.cbegin(),
        m_typedPluginManifest.outputEventTypes.cend(),
        [&qName](const nx::api::VcaAnalyticsEventType& event)
        {
            return event.internalName == qName;
        });

    return
        (it != m_typedPluginManifest.outputEventTypes.cend())
        ? *it
        : m_emptyEvent;
}

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
