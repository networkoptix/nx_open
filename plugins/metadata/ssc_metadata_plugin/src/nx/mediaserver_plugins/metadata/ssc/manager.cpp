#include "manager.h"

#include <QtCore/QString>

#include <nx/fusion/model_functions.h>

#include <nx/api/analytics/device_manifest.h>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "log.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace ssc {

namespace {

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const AnalyticsEventType& event, int logicalId)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setTypeId(
        nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(event.typeId));
    commonEvent->setDescription(event.name.value.toStdString());
    commonEvent->setAuxilaryData(std::to_string(logicalId));

    packet->addEvent(commonEvent);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

} // namespace

Manager::Manager(Plugin* plugin,
    const nx::sdk::CameraInfo& cameraInfo,
    const AnalyticsDriverManifest& typedManifest)
    :
    m_plugin(plugin),
    m_url(cameraInfo.url),
    m_cameraLogicalId(cameraInfo.logicalId)
{
    nx::api::AnalyticsDeviceManifest typedCameraManifest;
    for (const auto& eventType: typedManifest.outputEventTypes)
        typedCameraManifest.supportedEventTypes.push_back(eventType.typeId);
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    NX_URL_PRINT << "SSC metadata manager created for camera " << cameraInfo.model;
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "SSC metadata manager destroyed";
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

void Manager::sendEventPacket(const AnalyticsEventType& event) const
{
    ++m_packetId;
    auto packet = createCommonEventMetadataPacket(event, m_cameraLogicalId);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_URL_PRINT << "Event [pulse] packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData() << " sent to server";
}

nx::sdk::Error Manager::startFetchingMetadata(nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    m_plugin->registerCamera(m_cameraLogicalId, this);
    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    m_plugin->unregisterCamera(m_cameraLogicalId);
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
{
    return m_cameraManifest;
}

void Manager::freeManifest(const char* /*data*/)
{
    // Do nothing. Manifest string is stored in member-variable.
}

sdk::Error Manager::setHandler(sdk::metadata::MetadataHandler* handler)
{
    m_handler = handler;
    return sdk::Error::noError;
}

} // namespace ssc
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
