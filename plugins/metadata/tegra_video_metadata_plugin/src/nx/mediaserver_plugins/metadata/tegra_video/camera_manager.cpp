#include "camera_manager.h"

#include <iostream>
#include <chrono>

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_object.h>
#include <nx/mediaserver_plugins/utils/uuid.h>

#include "tegra_video_metadata_plugin_ini.h"
#include "attribute_options.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

namespace {

// TODO: #dmishin get rid of this. Pass object type id from TegraVideo.
static const QnUuid kCarUuid("{58AE392F-8516-4B27-AEE1-311139B5A37A}");
//kCarObjectType = "visionlabs.objectType.Car"
//kCarObjectType = "nvidia.metropolis.objectType.Car"

static const QnUuid kHumanUuid("{58AE392F-8516-4B27-AEE1-311139B5A37A}");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

CameraManager::CameraManager(Plugin* plugin):
    CommonVideoFrameProcessingCameraManager(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
    NX_PRINT << __func__ << "() BEGIN -> " << this;

    ini().reload();

    m_tegraVideo.reset(tegraVideoCreate());
    if (!m_tegraVideo)
    {
        NX_PRINT << "ERROR: Unable to create instance of TegraVideo.";
        // Not much to do about this error.
    }

    if (strcmp(ini().postprocType, "ped") == 0)
    {
        setTrackerAttributeOptions(kHumanAttributeOptions);
        m_tracker.setObjectTypeId(kHumanUuid);
    }
    else if (strcmp(ini().postprocType, "car") == 0)
    {
        setTrackerAttributeOptions(kCarAttributeOptions);
        m_tracker.setObjectTypeId(kCarUuid);
    }

    // TODO: Move all the ctor code below to startFetchingMetadata() when it starts being called.

    stopFetchingMetadata();

    TegraVideo::Params params;
    params.id = "metadata";
    params.deployFile = ini().deployFile;
    params.modelFile = ini().modelFile;
    params.cacheFile = ini().cacheFile;
    params.netWidth = ini().netWidth;
    params.netHeight = ini().netHeight;

    if (!m_tegraVideo->start(&params))
        NX_PRINT << "ERROR: TegraVideo::start() failed.";

    NX_OUTPUT << __func__ << "() END -> " << this;
}

CameraManager::~CameraManager()
{
    NX_OUTPUT << __func__ << "(" << this << ") BEGIN";

    if (!m_tegraVideo->stop())
        NX_OUTPUT << __func__ << "(): ERROR: TegraVideo::stop() failed";

    NX_OUTPUT << __func__ << "(" << this << ") END";
}

std::string CameraManager::capabilitiesManifest()
{
    return R"manifest(
        {
             "supportedObjectTypes": [
                "{58AE392F-8516-4B27-AEE1-311139B5A37A}",
                "{3778A599-FB60-47E9-8EC6-A9949E8E0AE7}"
            ]
        }
    )manifest";
}

bool CameraManager::pushCompressedFrame(const CompressedVideoPacket* videoPacket)
{
    TegraVideo::CompressedFrame compressedFrame;
    compressedFrame.dataSize = videoPacket->dataSize();
    compressedFrame.data = (const uint8_t*) videoPacket->data();
    compressedFrame.ptsUs = videoPacket->timestampUsec();

    NX_OUTPUT << "Pushing frame to net: PTS " << compressedFrame.ptsUs;
    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        NX_PRINT << "ERROR: TegraVideo::pushCompressedFrame() failed";
        return false;
    }
    return true;
}

bool CameraManager::pullRectsForFrame(std::vector<TegraVideo::Rect>* rects, int64_t* outPtsUs)
{
    static constexpr int kMaxRects = 1000;
    rects->resize(kMaxRects);

    *outPtsUs = -1;
    int rectsCount = -1;
    if (!m_tegraVideo->pullRectsForFrame(
        &rects->front(), (int) rects->size(), &rectsCount, outPtsUs))
    {
        NX_PRINT << "ERROR: TegraVideo::pullRectsForFrame() failed";
        return false;
    }

    rects->resize(rectsCount);
    return true;
}

bool CameraManager::pushCompressedVideoFrame(const CompressedVideoPacket* videoFrame)
{
    if (!pushCompressedFrame(videoFrame))
        return false;
    m_lastFramePtsUs = videoFrame->timestampUsec();
    return true;
}

bool CameraManager::pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
{
    while (m_tegraVideo->hasMetadata())
    {
        std::vector<TegraVideo::Rect> rects;
        int64_t ptsUs;
        if (!pullRectsForFrame(&rects, &ptsUs))
            return false;

        if (rects.empty())
            return true; //< No error: no rects at this time.

        if (NX_DEBUG_ENABLE_OUTPUT)
        {
            const int64_t dPts = ptsUs - m_lastFramePtsUs;
            const std::string dPtsMsStr =
                (dPts >= 0 ? "+" : "-") + nx::kit::debug::format("%lld", (500 + abs(dPts)) / 1000);

            NX_OUTPUT << "Got " << rects.size() << " rect(s) for PTS " << ptsUs
                << " (" << dPtsMsStr << " ms):";

            for (const auto rect: rects)
            {
                NX_OUTPUT << "    x " << rect.x << ", y " << rect.y
                    << ", w " << rect.w << ", h " << rect.h;
            }
        }

        if (!makeMetadataPacketsFromRects(metadataPackets, rects, ptsUs))
            NX_OUTPUT << "WARNING: makeMetadataPacketsFromRects() failed";
    }
    return true; //< No error: no more metadata.
}

//-------------------------------------------------------------------------------------------------
// private

bool CameraManager::makeMetadataPacketsFromRects(
    std::vector<nx::sdk::metadata::MetadataPacket*>* metadataPackets,
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

bool CameraManager::makeMetadataPacketsFromRectsPostprocNone(
    std::vector<nx::sdk::metadata::MetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // Create metadata Objects directly from the rects; create no events.

    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(ptsUs);
    objectPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    for (const auto& rect: rects)
    {
        auto commonObject = new CommonObject();
        const auto objectId =
            nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(QnUuid::createUuid());
        commonObject->setId(objectId);
        commonObject->setTypeId(m_objectTypeId);

        commonObject->setBoundingBox(Rect(rect.x, rect.y, rect.w, rect.h));
        objectPacket->addItem(commonObject);
    }
    metadataPackets->push_back(objectPacket);

    NX_OUTPUT << __func__ << "(): Created objects packet with " << rects.size() << " rects";
    return true;
}

bool CameraManager::makeMetadataPacketsFromRectsPostprocPed(
    std::vector<nx::sdk::metadata::MetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // TODO: #dmishin: STUB
    return makeMetadataPacketsFromRectsPostprocNone(metadataPackets, rects, ptsUs);
}

bool CameraManager::makeMetadataPacketsFromRectsPostprocCar(
    std::vector<nx::sdk::metadata::MetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    auto objectPacket = new CommonObjectsMetadataPacket();
    objectPacket->setTimestampUsec(ptsUs);
    objectPacket->setDurationUsec(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    m_tracker.filterAndTrack(metadataPackets, rects, ptsUs);

    return true;
}

int64_t CameraManager::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

void CameraManager::setTrackerAttributeOptions(
    const std::map<QString, std::vector<QString>>& options)
{
    for (const auto& entry : options)
    {
        const auto attributeName = entry.first;
        const auto& attributeOptions = entry.second;

        m_tracker.setAttributeOptions(attributeName, attributeOptions);
    }
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
