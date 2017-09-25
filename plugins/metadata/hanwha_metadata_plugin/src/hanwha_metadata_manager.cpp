#include "hanwha_metadata_manager.h"
#include "hanwha_common.h"
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

HanwhaMetadataManager::HanwhaMetadataManager()
{
}

HanwhaMetadataManager::~HanwhaMetadataManager()
{
    stopFetchingMetadata();
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

Error HanwhaMetadataManager::startFetchingMetadata(AbstractMetadataHandler* handler)
{
    decltype(m_monitor) monitor;

    auto monitorHandler = [this](const HanwhaEventList& events)
    {
        using namespace std::chrono;

        auto packet = new CommonEventMetadataPacket();

        std::cout << "------------------------------------------------------------------------------" << std::endl;
        for (const auto& hanwhaEvent : events)
        {
            auto event = new CommonDetectedEvent();
            std::cout
                << "Got event:"
                << hanwhaEvent.caption.toStdString() << " "
                << hanwhaEvent.description.toStdString() << std::endl;

            event->setEventTypeId(hanwhaEvent.typeId);
            event->setCaption(hanwhaEvent.caption.toStdString());
            event->setDescription(hanwhaEvent.caption.toStdString());
            event->setIsActive(hanwhaEvent.isActive);
            event->setConfidence(1.0);

            packet->setTimestampUsec(
                duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

            packet->setDurationUsec(-1);
            packet->addEvent(event);
        }
        std::cout << std::endl << std::endl;
        m_handler->handleMetadata(Error::noError, packet);
    };

    m_handler = handler;
    m_monitor = std::make_unique<HanwhaMetadataMonitor>(m_driverManifest, m_url, m_auth);
    m_monitor->setHandler(monitorHandler);
    m_monitor->startMonitoring();
    

    return Error::noError;
}

Error HanwhaMetadataManager::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->stopMonitoring();

    m_monitor.reset();

    return Error::noError;
}

const char* HanwhaMetadataManager::capabilitiesManifest(Error* error) const
{
    if (m_deviceManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceManifest.constData();
}

void HanwhaMetadataManager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
{
    m_url = resourceInfo.url;
    m_model = resourceInfo.model;
    m_firmware = resourceInfo.firmware;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);
}

void HanwhaMetadataManager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void HanwhaMetadataManager::setDriverManifest(const Hanwha::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
