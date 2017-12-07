#include "manager.h"

#include <iostream>
#include <chrono>

#define NX_PRINT_PREFIX "metadata::tegra_video::Manager::"
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_detected_object.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

#include "tegra_video_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

namespace {

// TODO: #dmishin get rid of this. Pass object type id from TegraVideo.
const QnUuid kCarUuid("{58AE392F-8516-4B27-AEE1-311139B5A37A}");
const QnUuid kHumanUuid("{58AE392F-8516-4B27-AEE1-311139B5A37A}");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager(Plugin* plugin):
    m_plugin(plugin)
{
    NX_PRINT << __func__ << "(\"" << plugin->name() << "\") BEGIN";

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
        NX_PRINT << "ERROR: TegraVideo::start() failed.";

    NX_OUTPUT << __func__ << "() END -> " << this;

    if (strcmp(ini().postprocType, "ped") == 0)
        m_tracker.setObjectTypeId(kHumanUuid);
    else if (strcmp(ini().postprocType, "car") == 0)
        m_tracker.setObjectTypeId(kCarUuid);
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

Error Manager::putData(AbstractDataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    std::vector<AbstractMetadataPacket*> metadataPackets;
    if (!pushFrameAndGetMetadataPackets(&metadataPackets, dataPacket))
    {
        NX_OUTPUT << __func__ << "() END -> unknownError";
        return Error::unknownError;
    }

    if (!metadataPackets.empty())
    {
        NX_OUTPUT << __func__ << "() Producing " << metadataPackets.size()
            << " metadata packet(s)";
    }

    for (auto& metadataPacket: metadataPackets)
    {
        m_handler->handleMetadata(Error::noError, metadataPacket);
        metadataPacket->releaseRef();
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
            "supportedEventTypes": [
                "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}"
            ],
            "supportedObjectTypes": [
                "{58AE392F-8516-4B27-AEE1-311139B5A37A}",
                "{3778A599-FB60-47E9-8EC6-A9949E8E0AE7}"
            ]
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

bool Manager::makeMetadataPacketsFromRects(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    if (strcmp(ini().postprocType, "ped") == 0)
        return makeMetadataPacketsFromRectsPostprocPed(metadataPackets, rects, ptsUs);

    if (strcmp(ini().postprocType, "car") == 0)
        return makeMetadataPacketsFromRectsPostprocCar(metadataPackets, rects, ptsUs);

    if (strcmp(ini().postprocType, "none") != 0)
    {
        NX_PRINT << "WARNING: Invalid .ini netPostprocType=\"" << ini().postprocType
            << "\"; assuming \"none\".";
    }

    return makeMetadataPacketsFromRectsPostprocNone(metadataPackets, rects, ptsUs);
}

bool Manager::makeMetadataPacketsFromRectsPostprocNone(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // Create metadata Objects directly from the rects; create no events.

    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(ptsUs);
    objectPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    for (const auto& rect: rects)
    {
        auto detectedObject = new CommonDetectedObject();

        const auto objectId = nxpt::fromQnUuidToPluginGuid(QnUuid::createUuid());
        detectedObject->setId(objectId);
        detectedObject->setTypeId(m_objectTypeId);

        detectedObject->setBoundingBox(Rect(rect.x, rect.y, rect.width, rect.height));
        objectPacket->addItem(detectedObject);
    }
    metadataPackets->push_back(objectPacket);

    NX_OUTPUT << __func__ << "(): Created objects packet with " << rects.size() << " rects";
    return true;
}

bool Manager::makeMetadataPacketsFromRectsPostprocPed(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // TODO: #dmishin: STUB

    // Create metadata Objects directly from the rects; create no events.

    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(ptsUs);
    objectPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    for (const auto& rect: rects)
    {
        auto detectedObject = new CommonDetectedObject();

        const auto objectId = nxpt::fromQnUuidToPluginGuid(QnUuid::createUuid());
        detectedObject->setId(objectId);
        detectedObject->setTypeId(m_objectTypeId);

        detectedObject->setBoundingBox(Rect(rect.x, rect.y, rect.width, rect.height));
        objectPacket->addItem(detectedObject);
    }
    metadataPackets->push_back(objectPacket);
    return true;
}

bool Manager::makeMetadataPacketsFromRectsPostprocCar(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // TODO: #dmishin: STUB

    // Create metadata Objects directly from the rects; create no events.

    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(ptsUs);
    objectPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    m_tracker.filterAndTrack(metadataPackets, rects, ptsUs);

#if 0 //< Stub implementation.
    for (const auto& rect: rects)
    {
        auto detectedObject = new CommonDetectedObject();
        // TODO: #mike: Make new GUID for every object.
        static const nxpl::NX_GUID objectId =
            {{0xB5, 0x29, 0x4F, 0x25, 0x4F, 0xE6, 0x46, 0x47, 0xB8, 0xD1, 0xA0, 0x72, 0x9F, 0x70, 0xF2, 0xD1}};

        detectedObject->setId(objectId);
        detectedObject->setTypeId(m_objectTypeId);

        detectedObject->setBoundingBox(Rect(rect.x, rect.y, rect.width, rect.height));
        objectPacket->addItem(detectedObject);
    }
    metadataPackets->push_back(objectPacket);
#endif // 0

    return true;
}

bool Manager::pushFrameAndGetMetadataPackets(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
    AbstractDataPacket* mediaPacket) const
{
    nxpt::ScopedRef<CommonCompressedVideoPacket> videoPacket =
        (CommonCompressedVideoPacket*) mediaPacket->queryInterface(IID_CompressedVideoPacket);
    if (!videoPacket)
        return true; //< No error: no video frame.

    TegraVideo::CompressedFrame compressedFrame;
    compressedFrame.dataSize = videoPacket->dataSize();
    compressedFrame.data = (const uint8_t*) videoPacket->data();
    compressedFrame.ptsUs = videoPacket->timestampUsec();

    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        NX_PRINT << "ERROR: TegraVideo::pushCompressedFrame() failed";
        return false;
    }

    if (!m_tegraVideo->hasMetadata())
        return true; //< No error: no metadata at this time.

    static constexpr int kMaxRects = 1000;
    std::vector<TegraVideo::Rect> rects;
    rects.resize(kMaxRects);

    int64_t ptsUs = -1;
    int rectsCount = -1;
    if (!m_tegraVideo->pullRectsForFrame(&rects.front(), rects.size(), &rectsCount, &ptsUs))
    {
        NX_PRINT << "ERROR: TegraVideo::pullRectsForFrame() failed";
        return false;
    }

    if (rectsCount <= 0)
        return true; //< No error: no rects at this time.

    rects.resize(rectsCount);
    if (NX_DEBUG_ENABLE_OUTPUT)
    {
        NX_OUTPUT << "Got " << rectsCount << " rect(s) for PTS " << ptsUs << ":";
        for (const auto rect: rects)
        {
            NX_OUTPUT << "    x " << rect.x << ", y " << rect.y
                << ", width " << rect.width << ", height " << rect.height;
        }
    }

    return makeMetadataPacketsFromRects(metadataPackets, rects, ptsUs);
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
