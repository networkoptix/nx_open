#include "hanwha_common.h"
#include "hanwha_metadata_manager.h"
#include "hanwha_attributes_parser.h"

#include <QtCore/QUrl>

#include <chrono>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/utils/log/log.h>

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

Error HanwhaMetadataManager::startFetchingMetadata(
    MetadataHandler* handler,
    nxpl::NX_GUID* /*typeList*/,
    int /*typeListSize*/)
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

                auto event = new CommonEvent();
                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channel));

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

void HanwhaMetadataManager::setCameraInfo(const nx::sdk::CameraInfo& cameraInfo)
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
