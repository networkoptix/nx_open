#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

Manager::Manager(
    const nx::sdk::ResourceInfo& resourceInfo,
    const QList<IdentifiedSupportedEvent>& events)
{
    m_url = resourceInfo.url;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);

    nx::api::AnalyticsDeviceManifest deviceManifest;
    for (const auto& event : events)
    {
        deviceManifest.supportedEventTypes.push_back(event.internalTypeId());
    }

    // We use m_deviceManifestFull to give server full manifest, m_deviceManifestPartial (that
    // gives server only Uuids) may be users for test purposes.
    //m_deviceManifestPartial = QJson::serialized(deviceManifest);

    m_deviceManifestFull = serializeEvents(events).toUtf8();

    m_identifiedSupportedEvents = events;

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
    if (m_deviceManifestFull.isEmpty())
    {
        *error = nx::sdk::Error::unknownError;
        return nullptr;
    }
    *error = nx::sdk::Error::noError;
    m_givenManifests.push_back(m_deviceManifestFull);
    return m_deviceManifestFull.constData();
}

void Manager::freeManifest(const char* data)
{
    // Do nothing. Memory allocated for Manifests is stored in m_givenManifests list and will be
    // released in Manager's destructor.
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
