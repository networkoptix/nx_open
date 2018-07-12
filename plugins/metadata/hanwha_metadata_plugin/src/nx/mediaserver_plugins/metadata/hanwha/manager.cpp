#define NX_PRINT_PREFIX "[metadata::hanwha::Manager] "
#include <nx/kit/debug.h>

#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/utils/log/log.h>

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

nx::sdk::Error Manager::setHandler(MetadataHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

void Manager::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // There are no Manager settings for this plugin.
}

Error Manager::startFetchingMetadata(nxpl::NX_GUID* /*typeList*/, int /*typeListSize*/)
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

                auto event = new CommonEvent();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channel;

                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channel));

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

            NX_ASSERT(m_handler, "Handler should exist");
            if (m_handler)
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

void Manager::freeManifest(const char* data)
{
}

void Manager::setCameraInfo(const nx::sdk::CameraInfo& cameraInfo)
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
