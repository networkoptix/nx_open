#include "hikvision_common.h"
#include "hikvision_metadata_manager.h"

#include <QtCore/QUrl>

#include <chrono>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <plugins/plugin_internal_tools.h>

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

HikvisionMetadataManager::HikvisionMetadataManager(HikvisionMetadataPlugin* plugin):
    m_plugin(plugin)
{
}

HikvisionMetadataManager::~HikvisionMetadataManager()
{
    stopFetchingMetadata();
}

void* HikvisionMetadataManager::queryInterface(const nxpl::NX_GUID& interfaceId)
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

Error HikvisionMetadataManager::startFetchingMetadata(
    AbstractMetadataHandler* handler,
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
                std::cout
                    << "---------------- (Metadata manager handler) Got event: "
                    << hikvisionEvent.caption.toStdString() << " "
                    << hikvisionEvent.description.toStdString() << " "
                    << "Channel " << m_channel << std::endl;

                event->setEventTypeId(
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

            std::cout << std::endl << std::endl;
            m_handler->handleMetadata(Error::noError, packet);
        };

    NX_ASSERT(m_plugin);
    std::vector<QnUuid> eventTypes;
    for (int i = 0; i < eventTypeListSize; ++i)
        eventTypes.push_back(nxpt::fromPluginGuidToQnUuid(eventTypeList[i]));
    m_monitor =
        std::make_unique<HikvisionMetadataMonitor>(m_plugin->driverManifest(), m_url, m_auth, eventTypes);

    m_handler = handler;

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

Error HikvisionMetadataManager::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_plugin);

    m_monitor = nullptr;
    m_handler = nullptr;

    return Error::noError;
}

const char* HikvisionMetadataManager::capabilitiesManifest(Error* error) const
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

void HikvisionMetadataManager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
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

void HikvisionMetadataManager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void HikvisionMetadataManager::setDriverManifest(const Hikvision::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
