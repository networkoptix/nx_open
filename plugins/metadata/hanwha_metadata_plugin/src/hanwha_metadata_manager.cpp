#include "hanwha_common.h"
#include "hanwha_metadata_manager.h"
#include "hanwha_attributes_parser.h"

#include <QtCore/QUrl>

#include <chrono>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

HanwhaMetadataManager::HanwhaMetadataManager(HanwhaMetadataPlugin* plugin):
    m_plugin(plugin)
{
}

HanwhaMetadataManager::~HanwhaMetadataManager()
{
    stopFetchingMetadata();
    m_plugin->managerIsAboutToBeDestroyed(m_sharedId);
}

void* HanwhaMetadataManager::queryInterface(const nxpl::NX_GUID& interfaceId)
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

Error HanwhaMetadataManager::startFetchingMetadata(
    AbstractMetadataHandler* handler,
    nxpl::NX_GUID* /*eventTypeList*/,
    int /*eventTypeListSize*/)
{
    auto monitorHandler =
        [this](const HanwhaEventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventMetadataPacket();

            for (const auto& hanwhaEvent : events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channel)
                    return;

                auto event = new CommonDetectedEvent();
                std::cout
                    << "---------------- (Metadata manager handler) Got event: "
                    << hanwhaEvent.caption.toStdString() << " "
                    << hanwhaEvent.description.toStdString() << " "
                    << "Channel " << m_channel << std::endl;

                event->setEventTypeId(hanwhaEvent.typeId);
                event->setCaption(hanwhaEvent.caption.toStdString());
                event->setDescription(hanwhaEvent.caption.toStdString());
                event->setIsActive(hanwhaEvent.isActive);
                event->setConfidence(1.0);
                event->setAuxilaryData(hanwhaEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addEvent(event);
            }

            std::cout << std::endl << std::endl;
            m_handler->handleMetadata(Error::noError, packet);
        };

    NX_ASSERT(m_plugin);
    m_monitor = m_plugin->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return Error::unknownError;

    m_handler = handler;

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

Error HanwhaMetadataManager::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_plugin);
    if (m_plugin)
        m_plugin->managerStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
    m_handler = nullptr;

    return Error::noError;
}

const char* HanwhaMetadataManager::capabilitiesManifest(Error* error)
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

void HanwhaMetadataManager::freeManifest(const char* data)
{
}

void HanwhaMetadataManager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
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

void HanwhaMetadataManager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void HanwhaMetadataManager::setDriverManifest(const Hanwha::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

void HanwhaMetadataManager::setMonitor(HanwhaMetadataMonitor* monitor)
{
    m_monitor = monitor;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
