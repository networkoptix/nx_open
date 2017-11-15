#include "manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#define NX_PRINT_PREFIX "tegra_video::Manager"
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_detected_object.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

#include "tegra_video_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

namespace {

static const nxpl::NX_GUID kLineCrossingEventGuid =
    {{0x7E, 0x94, 0xCE, 0x15, 0x3B, 0x69, 0x47, 0x19, 0x8D, 0xFD, 0xAC, 0x1B, 0x76, 0xE5, 0xD8, 0xF4}};

static const nxpl::NX_GUID kObjectInTheAreaEventGuid =
    {{0xB0, 0xE6, 0x40, 0x44, 0xFF, 0xA3, 0x4B, 0x7F, 0x80, 0x7A, 0x06, 0x0C, 0x1F, 0xE5, 0xA0, 0x4C}};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager():
    m_eventTypeId(kLineCrossingEventGuid)
{
    NX_OUTPUT << "Manager() BEGIN";
#if 0
    m_tegraVideo.reset(TegraVideo::create());
#endif // 0
    NX_OUTPUT << "Manager() END";
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    NX_OUTPUT << "queryInterface()";
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
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    return Error::noError;
}

Error Manager::startFetchingMetadata()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    stopFetchingMetadataUnsafe();

    auto metadataDigger =
        [this]()
        {
            while (!m_stopping)
            {
                m_handler->handleMetadata(Error::noError, cookSomeEvents());
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        };

    m_thread.reset(new std::thread(metadataDigger));
#if 0
    TegraVideo::Params params;
    params.deployFile = ini().deployFile;
    params.modelFile = ini().modelFile;
    params.cacheFile = ini().cacheFile;
    params.netWidth = ini().netWidth;
    params.netHeight = ini().netHeight;

    if (!m_tegraVideo->start(params))
        return Error::unknownError;
#endif // 0
    return Error::noError;
}

Error Manager::putData(
    AbstractDataPacket* dataPacket)
{
#if 0
    const auto metadata = pushFrameAndGetRects(dataPacket);
    if (metadata != nullptr)
        m_handler->handleMetadata(Error::noError, metadata);
#else // 0
    m_handler->handleMetadata(Error::noError, pushFrameAndGetRects(dataPacket));
#endif // 0
    return Error::noError;
}

Error Manager::stopFetchingMetadata()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return stopFetchingMetadataUnsafe();
}

const char* Manager::capabilitiesManifest(Error* error) const
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

Manager::~Manager()
{
    stopFetchingMetadata();
    std::cout << "Destroying metadata manager!" << (uintptr_t)this;
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
#if 0
    if (!m_tegraVideo->stop())
        return Error::unknownError;
#endif // 0
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
    detectedEvent->setAuxilaryData(R"json( {"auxilaryData": "someJson"} )json");
    detectedEvent->setIsActive(m_counter == 1);
    detectedEvent->setEventTypeId(m_eventTypeId);

    auto eventPacket = new CommonEventsMetadataPacket();
    eventPacket->setTimestampUsec(usSinceEpoch());
    eventPacket->addItem(detectedEvent);
    return eventPacket;
}

AbstractMetadataPacket* Manager::pushFrameAndGetRects(
    nx::sdk::metadata::AbstractDataPacket* mediaPacket)
{
#if 0
    nxpt::ScopedRef<CommonCompressedVideoPacket> videoPacket =
        (CommonCompressedVideoPacket*) mediaPacket->queryInterface(IID_CompressedVideoPacket);
    if (!videoPacket)
        return nullptr;

    TegraVideo::CompressedFrame compressedFrame;
    compressedFrame.dataSize = videoPacket->dataSize();
    compressedFrame.data = (const uint8_t*) videoPacket->data();
    compressedFrame.ptsUs = videoPacket->timestampUsec();

    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        NX_PRINT << "ERROR: TegraVideo::pushCompressedFrame() failed";
        return nullptr;
    }

    if (!m_tegraVideo->hasMetadata())
        return nullptr;

    static constexpr int kMaxRects = 1000;
    std::vector<TegraVideo::Rect> rects;
    rects.resize(kMaxRects);

    int64_t ptsUs = -1;
    int rectsCount = -1;
    if (!m_tegraVideo->pullRectsForFrame(&rects.front(), rects.size(), &rectsCount, &ptsUs))
    {
        NX_PRINT << "ERROR: TegraVideo::pullRectsForFrame() failed";
        return nullptr;
    }

    if (rectsCount <= 0)
        return nullptr;

    auto eventPacket = new CommonObjectsMetadataPacket();
    eventPacket->setTimestampUsec(ptsUs);
    eventPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    rects.resize(rectsCount);
    NX_OUTPUT << "Got " << rectsCount << " rects for PTS " << ptsUs;
    for (const auto rect: rects)
    {
        auto detectedObject = new CommonDetectedObject();
        static const nxpl::NX_GUID objectId =
            {{0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47, 0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

        detectedObject->setId(objectId);
////    detectedObject->setAuxilaryData(R"json( {"auxilaryData": "someJson2"} )json");
        detectedObject->setEventTypeId(m_objectTypeId);

        // ATTENTION: Here we use videoPacket frame size to calculate 0..1 coords, but this is a
        // size of the incoming frame, not the one for which rects were extracted.
        detectedObject->setBoundingBox(Rect(
            rect.x / videoPacket->width(),
            rect.y / videoPacket->height(),
            rect.width / videoPacket->width(),
            rect.height / videoPacket->height()));

        eventPacket->addItem(detectedObject);
    }

    return eventPacket;
#else // 0
    nxpt::ScopedRef<CommonCompressedVideoPacket> videoPacket =
        (CommonCompressedVideoPacket*) mediaPacket->queryInterface(IID_CompressedVideoPacket);
    if (!videoPacket)
        return nullptr;

    auto detectedObject = new CommonDetectedObject();
    static const nxpl::NX_GUID objectId =
        {{0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47, 0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

    detectedObject->setId(objectId);
    detectedObject->setAuxilaryData(R"json( {"auxilaryData": "someJson2"} )json");
    detectedObject->setEventTypeId(m_objectTypeId);

    double dt = m_counterObjects++ / 32.0;
    double intPart;
    dt = modf(dt, &intPart) * 0.75;

    detectedObject->setBoundingBox(Rect(dt, dt, 0.25, 0.25));

    auto eventPacket = new CommonObjectsMetadataPacket();
    eventPacket->setTimestampUsec(videoPacket->timestampUsec());
    eventPacket->setDurationUsec(1000000LL * 10);
    eventPacket->addItem(detectedObject);
    return eventPacket;
#endif // 0
}

int64_t Manager::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
