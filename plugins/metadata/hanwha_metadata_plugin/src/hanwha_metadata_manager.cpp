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
    m_manifest = QString(kPluginManifestTemplate)
        .arg(str(kHanwhaFaceDetectionEventId))
        .arg(str(kHanwhaVirtualLineEventId))
        .arg(str(kHanwhaEnteringEventId))
        .arg(str(kHanwhaExitingEventId))
        .arg(str(kHanwhaAppearingEventId))
        .arg(str(kHanwhaDisappearingEventId))
        .arg(str(kHanwhaAudioDetectionEventId))
        .arg(str(kHanwhaTamperingEventId))
        .arg(str(kHanwhaDefocusingEventId))
        .arg(str(kHanwhaDryContactInputEventId))
        .arg(str(kHanwhaMotionDetectionEventId))
        .arg(str(kHanwhaSoundClassificationEventId))
        .arg(str(kHanwhaLoiteringEventId)).toUtf8();
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
    QnMutexLocker lock(&m_mutex);
    m_handler = handler;
    m_monitor = std::make_unique<HanwhaMetadataMonitor>(m_url, m_auth);

    auto monitorHandler = [this] (const HanwhaEventList& events)
        {
            using namespace std::chrono;

            auto packet = new CommonEventMetadataPacket();
            
            std::cout << "------------------------------------------------------------------------------" << std::endl;
            for (const auto& hanwhaEvent: events)
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
    if (m_manifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_manifest.constData();
}

void HanwhaMetadataManager::setUrl(const QString& url)
{
    m_url = url;
}

void HanwhaMetadataManager::setModel(const QString& model)
{
    m_model = model;
}

void HanwhaMetadataManager::setFirmware(const QString& firmware)
{
    m_firmware = firmware;
}

void HanwhaMetadataManager::setAuth(const QAuthenticator& auth)
{
    m_auth = auth;
}

void HanwhaMetadataManager::setCapabilitiesManifest(const QByteArray& manifest)
{
    m_manifest = manifest;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx