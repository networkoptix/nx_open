#include "axis_metadata_manager.h"

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

using namespace nx::sdk;
using namespace nx::sdk::metadata;

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
    m_deviceManifest = QJson::serialized(deviceManifest);

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
    if (interfaceId == IID_MetadataManager)
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

Error Manager::startFetchingMetadata(AbstractMetadataHandler* handler,
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    m_monitor = new Monitor(this, m_url, m_auth, handler);
    return m_monitor->startMonitoring(eventTypeList, eventTypeListSize);
}

Error Manager::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
    return Error::noError;
}

const char* Manager::capabilitiesManifest(Error* error) const
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
