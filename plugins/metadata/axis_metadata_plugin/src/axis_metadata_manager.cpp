//#if defined(ENABLE_HANWHA)

#include "axis_metadata_manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

#include "axis_common.h"

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

AxisMetadataManager::AxisMetadataManager(
    const nx::sdk::ResourceInfo& resourceInfo,
    const QList<SupportedEventEx>& axisEvents)
{
    m_url = resourceInfo.url;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);

    nx::api::AnalyticsDeviceManifest deviceManifest;
    for (const auto& event : axisEvents)
    {
        deviceManifest.supportedEventTypes.push_back(event.internalTypeId());
    }
    m_deviceManifest = QJson::serialized(deviceManifest);

    m_axisEvents = axisEvents;

    NX_PRINT << "Ctor :" << this;
}

AxisMetadataManager::~AxisMetadataManager()
{
    stopFetchingMetadata();
    NX_PRINT << "Dtor :" << this;
}

void* AxisMetadataManager::queryInterface(const nxpl::NX_GUID& interfaceId)
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

Error AxisMetadataManager::startFetchingMetadata(AbstractMetadataHandler* handler,
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    m_monitor = new AxisMetadataMonitor(this, m_url, m_auth, handler);
    return m_monitor->startMonitoring(eventTypeList, eventTypeListSize);
}

Error AxisMetadataManager::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
    return Error::noError;
}

const char* AxisMetadataManager::capabilitiesManifest(Error* error) const
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

//#endif // defined(ENABLE_HANWHA)