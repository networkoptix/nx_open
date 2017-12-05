#include "manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#define NX_PRINT_PREFIX "tegra_video::Manager::"
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
    NX_OUTPUT << __func__ << "() BEGIN";

    m_tegraVideo.reset(TegraVideo::create());

    // TODO: #mike: Move to a new start...() method after SDK is extended.
    TegraVideo::Params params;
    params.id = "metadata";
    params.deployFile = ini().deployFile;
    params.modelFile = ini().modelFile;
    params.cacheFile = ini().cacheFile;
    params.netWidth = ini().netWidth;
    params.netHeight = ini().netHeight;

    if (!m_tegraVideo->start(params))
    {
        NX_PRINT << "ERROR: TegraVideo::start() failed.";
        NX_OUTPUT << __func__ << "() END -> unknownError";
    }

    NX_OUTPUT << __func__ << "() END";
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
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    return Error::noError;
}

Error Manager::startFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    std::lock_guard<std::mutex> lock(m_mutex);

    stopFetchingMetadataThreadUnsafe();

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::putData(
    AbstractDataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    const auto packet = pushFrameAndGetRects(dataPacket);
    if (packet)
    {
        m_handler->handleMetadata(Error::noError, packet);
        packet->releaseRef();
    }

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";

    return Error::noError;
}

const char* Manager::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    return R"manifest(
        {
        }
    )manifest";
}

Manager::~Manager()
{
    NX_OUTPUT << __func__ << "() BEGIN";

    stopFetchingMetadata();

    Error result = stopFetchingMetadataThreadUnsafe();

    NX_OUTPUT << __func__ << "() END";
}

Error Manager::stopFetchingMetadataThreadUnsafe()
{
    if (!m_tegraVideo->stop())
        return Error::unknownError;
    return Error::noError;
}

AbstractMetadataPacket* Manager::pushFrameAndGetRects(
    nx::sdk::metadata::AbstractDataPacket* mediaPacket)
{
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
    NX_OUTPUT << "Got " << rectsCount << " rect(s) for PTS " << ptsUs << ":";
    for (const auto rect: rects)
    {
        auto detectedObject = new CommonDetectedObject();
        // TODO: #mike: Make new GUID for every object.
        static const nxpl::NX_GUID objectId =
            {{0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47, 0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

        detectedObject->setId(objectId);
        detectedObject->setTypeId(m_objectTypeId);

        NX_OUTPUT << "    x " << rect.x << ", y " << rect.y
            << ", width " << rect.width << ", height " << rect.height;

        detectedObject->setBoundingBox(Rect(rect.x, rect.y, rect.width, rect.height));
        eventPacket->addItem(detectedObject);
    }

    return eventPacket;
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
