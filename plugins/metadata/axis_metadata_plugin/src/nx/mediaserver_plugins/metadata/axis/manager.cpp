#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/serialization/json.h>

#define NX_PRINT_PREFIX "[metadata::axis::Manager] "
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

Manager::Manager(
    const nx::sdk::CameraInfo& cameraInfo,
    const AnalyticsDriverManifest& typedManifest)
    :
    m_typedManifest(typedManifest),
    m_manifest(QJson::serialized(typedManifest)),
    m_url(cameraInfo.url)
{
    m_auth.setUser(cameraInfo.login);
    m_auth.setPassword(cameraInfo.password);

    nx::api::AnalyticsDeviceManifest deviceManifest;
    for (const auto& eventType: typedManifest.outputEventTypes)
        deviceManifest.supportedEventTypes.push_back(eventType.id);
    NX_PRINT << "Axis metadata manager created";
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_PRINT << "Axis metadata manager destroyed";
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_CameraManager)
    {
        addRef();
        return static_cast<CameraManager*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nx::sdk::Error Manager::setHandler(nx::sdk::metadata::MetadataHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

void Manager::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // There are no Manager settings for this plugin.
}

nx::sdk::Error Manager::startFetchingMetadata(const char* const* typeList, int typeListSize)
{
    m_monitor = new Monitor(this, m_url, m_auth, m_handler);
    return m_monitor->startMonitoring(typeList, typeListSize);
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
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

void Manager::freeManifest(const char* data)
{
    // Do nothing. Memory allocated for Manifests is stored in m_givenManifests list and will be
    // released in Manager's destructor.
}

const AnalyticsEventType* Manager::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&id](const AnalyticsEventType& eventType) { return eventType.id == id; });

    if (it == m_typedManifest.outputEventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
