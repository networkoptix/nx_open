#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[metadata::hanwha::Manager] "
#include <nx/kit/debug.h>

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager(Plugin* plugin):
    m_plugin(plugin)
{
}

Manager::~Manager()
{
    stopFetchingMetadata();
    m_plugin->managerIsAboutToBeDestroyed(m_sharedId);
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
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

nx::sdk::Error Manager::setHandler(AbstractMetadataHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error Manager::startFetchingMetadata(
    nxpl::NX_GUID* /*eventTypeList*/,
    int /*eventTypeListSize*/)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventsMetadataPacket();

            for (const auto& hanwhaEvent : events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channel)
                    return;

                auto event = new CommonDetectedEvent();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channel;

                event->setTypeId(hanwhaEvent.typeId);
                event->setCaption(hanwhaEvent.caption.toStdString());
                event->setDescription(hanwhaEvent.caption.toStdString());
                event->setIsActive(hanwhaEvent.isActive);
                event->setConfidence(1.0);
                event->setAuxilaryData(hanwhaEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addItem(event);
            }

            NX_PRINT;
            m_handler->handleMetadata(Error::noError, packet);
            packet->releaseRef();
        };

    NX_ASSERT(m_plugin);
    m_monitor = m_plugin->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return Error::unknownError;

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

Error Manager::stopFetchingMetadata()
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

const char* Manager::capabilitiesManifest(Error* error)
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

void Manager::setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo)
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

void Manager::setDeviceManifest(const QByteArray& manifest)
{
    m_deviceManifest = manifest;
}

void Manager::setDriverManifest(const Hanwha::DriverManifest& manifest)
{
    m_driverManifest = manifest;
}

void Manager::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
}

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
