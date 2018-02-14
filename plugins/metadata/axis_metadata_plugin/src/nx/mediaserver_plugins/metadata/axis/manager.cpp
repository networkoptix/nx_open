#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

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
    for (const auto& event: typedManifest.outputEventTypes)
        deviceManifest.supportedEventTypes.push_back(event.eventTypeId);
}

Manager::~Manager()
{
    stopFetchingMetadata();
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

nx::sdk::Error Manager::startFetchingMetadata(nx::sdk::metadata::MetadataHandler* handler,
    nxpl::NX_GUID* typeList, int typeListSize)
{
    m_monitor = new Monitor(this, m_url, m_auth, handler);
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

const AnalyticsEventType* Manager::eventByUuid(const QnUuid& uuid) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&uuid](const AnalyticsEventType& event) { return event.eventTypeId == uuid; });

    if (it == m_typedManifest.outputEventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
