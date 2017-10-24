#include "stub_metadata_manager.h"

#include <iostream>
#include <chrono>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_detected_object.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const nxpl::NX_GUID kLineCrossingEventGuid
    = {{0x7E, 0x94, 0xCE, 0x15, 0x3B, 0x69, 0x47, 0x19, 0x8D, 0xFD, 0xAC, 0x1B, 0x76, 0xE5, 0xD8, 0xF4}};

static const nxpl::NX_GUID kObjectInTheAreaEventGuid
    = {{0xB0, 0xE6, 0x40, 0x44, 0xFF, 0xA3, 0x4B, 0x7F, 0x80, 0x7A, 0x06, 0x0C, 0x1F, 0xE5, 0xA0, 0x4C}};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

StubMetadataManager::StubMetadataManager():
    m_eventTypeId(kLineCrossingEventGuid)
{
    std::cout << "Creating metadata manager! " << (uintptr_t)this << std::endl;
}

void* StubMetadataManager::queryInterface(const nxpl::NX_GUID& interfaceId)
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

Error StubMetadataManager::startFetchingMetadata(AbstractMetadataHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    stopFetchingMetadataUnsafe();
    m_handler = handler;

    auto metadataDigger = [this]()
        {
            while (!m_stopping)
            {
                m_handler->handleMetadata(Error::noError, cookSomeEvents());
                m_handler->handleMetadata(Error::noError, cookSomeObjects());
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        };

    m_thread.reset(new std::thread(metadataDigger));

    return Error::noError;
}

Error StubMetadataManager::stopFetchingMetadata()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return stopFetchingMetadataUnsafe();
}

const char* StubMetadataManager::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    return R"manifest(
        {
            "supportedEventTypes": [
                "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}"
            ]
        }
    )manifest";
}

StubMetadataManager::~StubMetadataManager()
{
    stopFetchingMetadata();
    std::cout << "Destroying metadata manager!" << (uintptr_t)this;
}


Error StubMetadataManager::stopFetchingMetadataUnsafe()
{
    m_stopping = true; //< looks like bsht
    if (m_thread)
    {
        m_thread->join();
        m_thread.reset();
    }
    m_stopping = false;

    return Error::noError;
}

AbstractMetadataPacket* StubMetadataManager::cookSomeEvents()
{
    ++m_counter;
    if (m_counter > 1)
    {
        if (m_eventTypeId == kLineCrossingEventGuid)
            m_eventTypeId = kObjectInTheAreaEventGuid;
        else
            m_eventTypeId = kLineCrossingEventGuid;

        m_counter = 0;
    }

    auto detectedEvent = new CommonDetectedEvent();
    detectedEvent->setCaption("Line crossing (caption)");
    detectedEvent->setDescription("Line crossing (description)");
    detectedEvent->setAuxilaryData(R"json({"auxilaryData": "someJson"})json");
    detectedEvent->setIsActive(m_counter == 1);
    detectedEvent->setEventTypeId(m_eventTypeId);

    auto eventPacket = new CommonMetadataPacket();
    eventPacket->setTimestampUsec(usSinceEpoch());
    eventPacket->addEvent(detectedEvent);
    return eventPacket;
}

AbstractMetadataPacket* StubMetadataManager::cookSomeObjects()
{
    auto detectedObject = new CommonDetectedObject();
    detectedObject->setAuxilaryData(R"json({"auxilaryData": "someJson2"})json");
    detectedObject->setEventTypeId(m_objectTypeId);
    detectedObject->setBoundingBox(Rect(0.25, 0.25, 0.25, 0.25));

    auto eventPacket = new CommonMetadataPacket();
    eventPacket->setTimestampUsec(usSinceEpoch());
    eventPacket->addEvent(detectedObject);
    return eventPacket;
}

int64_t StubMetadataManager::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
