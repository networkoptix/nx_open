#include "manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_detected_object.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

#define NX_DEBUG_ENABLE_OUTPUT true
#define NX_PRINT_PREFIX "metadata::stub::Manager::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

namespace {

static const nxpl::NX_GUID kLineCrossingEventGuid =
    {{0x7E, 0x94, 0xCE, 0x15, 0x3B, 0x69, 0x47, 0x19, 0x8D, 0xFD, 0xAC, 0x1B, 0x76, 0xE5, 0xD8, 0xF4}};

static const nxpl::NX_GUID kObjectInTheAreaEventGuid =
    {{0xB0, 0xE6, 0x40, 0x44, 0xFF, 0xA3, 0x4B, 0x7F, 0x80, 0x7A, 0x06, 0x0C, 0x1F, 0xE5, 0xA0, 0x4C}};

static const nxpl::NX_GUID kCarDetectedEventGuid =
    {{ 0x15, 0x3D, 0xD8, 0x79, 0x1C, 0xD2, 0x46, 0xB7, 0xAD, 0xD6, 0x7C, 0x6B, 0x48, 0xEA, 0xC1, 0xFC }};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager(Plugin* plugin):
    m_plugin(plugin),
    m_eventTypeId(kLineCrossingEventGuid)
{
    NX_PRINT << __func__ << "(\"" << plugin->name() << "\") -> " << this;
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataManager)
    {
        addRef();
        return static_cast<AbstractMetadataManager*>(this);
    }

    if (interfaceId == IID_ConsumingMetadataManager)
    {
        addRef();
        return static_cast<AbstractConsumingMetadataManager*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error Manager::setHandler(AbstractMetadataHandler* handler)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::startFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    std::lock_guard<std::mutex> lock(m_mutex);

    stopFetchingMetadataUnsafe();

    auto metadataDigger =
        [this]()
        {
            while (!m_stopping)
            {
                const auto packet = cookSomeEvents();
                m_handler->handleMetadata(Error::noError, packet);
                packet->releaseRef();
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        };

    m_thread.reset(new std::thread(metadataDigger));

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

nx::sdk::Error Manager::putData(nx::sdk::metadata::AbstractDataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    m_handler->handleMetadata(Error::noError, cookSomeObjects(dataPacket));
    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    std::lock_guard<std::mutex> lock(m_mutex);
    const Error result = stopFetchingMetadataUnsafe();
    if (result == Error::noError)
        NX_OUTPUT << __func__ << "() END -> noError";
    else
        NX_OUTPUT << __func__ << "() END -> Error{" << (int) result << "}";
    return result;
}

const char* Manager::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    // TODO: Reuse GUID constants declared in this file.
    return R"json(
        {
            "supportedEventTypes": [
                "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}"
            ],
            "supportedObjectTypes": [
                "{153DD879-1CD2-46B7-ADD6-7C6B48EAC1FC}"
            ]
        }
    )json";
}

Manager::~Manager()
{
    NX_PRINT << __func__ << "(" << this << ") BEGIN";
    stopFetchingMetadata();
    NX_PRINT << __func__ << "(" << this << ") END";
}

Error Manager::stopFetchingMetadataUnsafe()
{
    m_stopping = true; //< looks bad
    if (m_thread)
    {
        m_thread->join();
        m_thread.reset();
    }
    m_stopping = false;

    return Error::noError;
}

AbstractMetadataPacket* Manager::cookSomeEvents()
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
    detectedEvent->setAuxilaryData(R"json({ "auxilaryData": "someJson" })json");
    detectedEvent->setIsActive(m_counter == 1);
    detectedEvent->setTypeId(m_eventTypeId);

    auto eventPacket = new CommonEventsMetadataPacket();
    eventPacket->setTimestampUsec(usSinceEpoch());
    eventPacket->addItem(detectedEvent);
    return eventPacket;
}

AbstractMetadataPacket* Manager::cookSomeObjects(
    nx::sdk::metadata::AbstractDataPacket* mediaPacket)
{
    nxpt::ScopedRef<CommonCompressedVideoPacket> videoPacket =
        (CommonCompressedVideoPacket*) mediaPacket->queryInterface(IID_CompressedVideoPacket);
    if (!videoPacket)
        return nullptr;

    auto detectedObject = new CommonDetectedObject();
    static const nxpl::NX_GUID objectId =
        {{0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47, 0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

    detectedObject->setId(objectId);
    detectedObject->setAuxilaryData(R"json({ "auxilaryData": "someJson2" })json");
    detectedObject->setTypeId(kCarDetectedEventGuid);

    double dt = m_counterObjects++ / 32.0;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;

    detectedObject->setBoundingBox(Rect(dt, dt, 0.25, 0.25));

    auto eventPacket = new CommonObjectsMetadataPacket();
    eventPacket->setTimestampUsec(videoPacket->timestampUsec());
    eventPacket->setDurationUsec(1000000LL * 10);
    eventPacket->addItem(detectedObject);
    return eventPacket;
}

int64_t Manager::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
