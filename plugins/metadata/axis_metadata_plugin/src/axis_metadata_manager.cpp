//#if defined(ENABLE_HANWHA)

#include "axis_metadata_manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>

#include "axis_common.h"

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

AxisMetadataManager::AxisMetadataManager(AxisMetadataPlugin* plugin):
    m_plugin(plugin)
{
}

AxisMetadataManager::~AxisMetadataManager()
{
    stopFetchingMetadata();
    m_plugin->managerIsAboutToBeDestroyed(m_sharedId);
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

nx::sdk::Error AxisMetadataManager::setHandler(AbstractMetadataHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error AxisMetadataManager::startFetchingMetadata(nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    NX_ASSERT(m_plugin);
    if (m_plugin)
        m_monitor = m_plugin->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return Error::unknownError;
    m_monitor->setManager(this);
    m_monitor->startMonitoring(eventTypeList, eventTypeListSize);
    return Error::noError;
}

Error AxisMetadataManager::stopFetchingMetadata()
{
    NX_ASSERT(m_plugin);
    if (m_plugin)
        m_plugin->managerStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
    m_handler = nullptr;
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

void AxisMetadataManager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
{
    m_url = resourceInfo.url;
    m_model = resourceInfo.model;
    m_firmware = resourceInfo.firmware;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);
    m_uniqueId = resourceInfo.uid;
    m_sharedId = resourceInfo.sharedId;
    m_channel = resourceInfo.channel;
}

void AxisMetadataManager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void AxisMetadataManager::setDriverManifest(const Axis::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

void AxisMetadataManager::setMonitor(AxisMetadataMonitor* monitor)
{
    m_monitor = monitor;
}

nx::sdk::metadata::AbstractMetadataHandler* AxisMetadataManager::metadataHandler()
{
    return m_handler;
}

AxisMetadataPlugin* AxisMetadataManager::plugin()
{
    return m_plugin;
}

QString AxisMetadataManager::sharedId()
{
    return m_sharedId;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

//#endif // defined(ENABLE_HANWHA)