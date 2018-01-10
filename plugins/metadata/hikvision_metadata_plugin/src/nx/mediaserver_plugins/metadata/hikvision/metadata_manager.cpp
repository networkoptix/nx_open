#include "common.h"
#include "metadata_manager.h"

#include <chrono>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <plugins/plugin_internal_tools.h>
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

nx::sdk::Error MetadataManager::setHandler(nx::sdk::metadata::AbstractMetadataHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

Error MetadataManager::startFetchingMetadata(
    nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    auto monitorHandler =
        [this](const HikvisionEventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventMetadataPacket();

            for (const auto& hikvisionEvent : events)
            {
                if (hikvisionEvent.channel.is_initialized() && hikvisionEvent.channel != m_channel)
                    return;

                auto event = new CommonDetectedEvent();
                NX_VERBOSE(this, lm("Got event: %1 %2 Channel %3")
                    .arg(hikvisionEvent.caption).arg(hikvisionEvent.description).arg(m_channel));

                event->setTypeId(
                    nxpt::NxGuidHelper::fromRawData(hikvisionEvent.typeId.toRfc4122()));
                event->setCaption(hikvisionEvent.caption.toStdString());
                event->setDescription(hikvisionEvent.caption.toStdString());
                event->setIsActive(hikvisionEvent.isActive);
                event->setConfidence(1.0);
                //event->setAuxilaryData(hikvisionEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addEvent(event);
            }

            m_handler->handleMetadata(Error::noError, packet);
        };

    NX_ASSERT(m_plugin);
    std::vector<QnUuid> eventTypes;
    for (int i = 0; i < eventTypeListSize; ++i)
        eventTypes.push_back(nxpt::fromPluginGuidToQnUuid(eventTypeList[i]));
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

const char* MetadataManager::capabilitiesManifest(Error* error) const
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

void MetadataManager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
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
