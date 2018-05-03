#include "common.h"
#include "metadata_manager.h"

#include <chrono>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

MetadataManager::MetadataManager(MetadataPlugin* plugin):
    m_plugin(plugin)
{
}

MetadataManager::~MetadataManager()
{
    stopFetchingMetadata();
}

void* MetadataManager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_CameraManager)
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

void MetadataManager::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // There are no Manager settings for this plugin.
}

nx::sdk::Error MetadataManager::setHandler(nx::sdk::metadata::MetadataHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

Error MetadataManager::startFetchingMetadata(
    nxpl::NX_GUID* typeList,
    int typeListSize)
{
    auto monitorHandler =
        [this](const HikvisionEventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventsMetadataPacket();

            for (const auto& hikvisionEvent : events)
            {
                if (hikvisionEvent.channel.is_initialized() && hikvisionEvent.channel != m_channel)
                    return;

                auto event = new CommonEvent();
                NX_VERBOSE(this, lm("Got event: %1 %2 Channel %3")
                    .arg(hikvisionEvent.caption).arg(hikvisionEvent.description).arg(m_channel));

                event->setTypeId(
                    nxpt::NxGuidHelper::fromRawData(hikvisionEvent.typeId.toRfc4122()));
                event->setCaption(hikvisionEvent.caption.toStdString());
                event->setDescription(hikvisionEvent.description.toStdString());
                event->setIsActive(hikvisionEvent.isActive);
                event->setConfidence(1.0);
                //event->setAuxilaryData(hikvisionEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addItem(event);
            }

            m_handler->handleMetadata(Error::noError, packet);
        };

    NX_ASSERT(m_plugin);
    std::vector<QnUuid> eventTypes;
    for (int i = 0; i < typeListSize; ++i)
        eventTypes.push_back(nx::mediaserver_plugins::utils::fromPluginGuidToQnUuid(typeList[i]));
    m_monitor =
        std::make_unique<HikvisionMetadataMonitor>(m_plugin->driverManifest(), m_url, m_auth, eventTypes);

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

Error MetadataManager::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_plugin);

    m_monitor = nullptr;
    m_handler = nullptr;

    return Error::noError;
}

const char* MetadataManager::capabilitiesManifest(Error* error)
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

void MetadataManager::freeManifest(const char* data)
{
}

void MetadataManager::setCameraInfo(const nx::sdk::CameraInfo& cameraInfo)
{
    m_url = cameraInfo.url;
    m_model = cameraInfo.model;
    m_firmware = cameraInfo.firmware;
    m_auth.setUser(cameraInfo.login);
    m_auth.setPassword(cameraInfo.password);
    m_uniqueId = cameraInfo.uid;
    m_sharedId = cameraInfo.sharedId;
    m_channel = cameraInfo.channel;
}

void MetadataManager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void MetadataManager::setDriverManifest(const Hikvision::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
