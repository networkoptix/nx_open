#include "device_agent.h"

#include <iostream>
#include <chrono>

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/analytics/helpers/object.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/vms_server_plugins/utils/uuid.h>

#include "tegra_video_analytics_plugin_ini.h"
#include "attribute_options.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    VideoFrameProcessingDeviceAgent(engine, NX_DEBUG_ENABLE_OUTPUT)
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
        m_tracker.setObjectTypeId("nx.tegraVideo.human");
    }
    else if (strcmp(ini().postprocType, "car") == 0)
    {
        setTrackerAttributeOptions(kCarAttributeOptions);
        m_tracker.setObjectTypeId("nx.tegraVideo.car");
    }

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

DeviceAgent::~DeviceAgent()
{
    NX_OUTPUT << __func__ << "(" << this << ") BEGIN";

    if (!m_tegraVideo->stop())
        NX_OUTPUT << __func__ << "(): ERROR: TegraVideo::stop() failed";

    NX_OUTPUT << __func__ << "(" << this << ") END";
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    NX_OUTPUT << __func__ << "() -> noError";
    return Error::noError;
}

std::string DeviceAgent::manifest() const
{
    return R"manifest(
        {
             "supportedObjectTypeIds": [
                "{58AE392F-8516-4B27-AEE1-311139B5A37A}",
                "{3778A599-FB60-47E9-8EC6-A9949E8E0AE7}"
            ]
        }
    )manifest";
}

bool DeviceAgent::pushCompressedFrame(const ICompressedVideoPacket* videoPacket)
{
    TegraVideo::CompressedFrame compressedFrame;
    compressedFrame.dataSize = videoPacket->dataSize();
    compressedFrame.data = (const uint8_t*) videoPacket->data();
    compressedFrame.ptsUs = videoPacket->timestampUs();

    NX_OUTPUT << "Pushing frame to net: PTS " << compressedFrame.ptsUs;
    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        NX_PRINT << "ERROR: TegraVideo::pushCompressedFrame() failed";
        return false;
    }
    return true;
}

bool DeviceAgent::pullRectsForFrame(std::vector<TegraVideo::Rect>* rects, int64_t* outPtsUs)
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

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    if (!pushCompressedFrame(videoFrame))
        return false;
    m_lastFramePtsUs = videoFrame->timestampUs();
    return true;
}

bool DeviceAgent::pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets)
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
                (dPts >= 0 ? "+" : "-") + nx::kit::utils::format("%lld", (500 + abs(dPts)) / 1000);

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

bool DeviceAgent::makeMetadataPacketsFromRects(
    std::vector<IMetadataPacket*>* metadataPackets,
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

bool DeviceAgent::makeMetadataPacketsFromRectsPostprocNone(
    std::vector<IMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // Create metadata Objects directly from the rects; create no events.

    auto objectPacket = new ObjectMetadataPacket();
    objectPacket->setTimestampUs(ptsUs);
    objectPacket->setDurationUs(1000000LL * 10); //< TODO: #mike: Ask #rvasilenko.

    for (const auto& rect: rects)
    {
        auto object = new Object();
        // TODO: #mshevchenko: Rewrite creating a random nx::sdk::Uuid when implemented.
        const nx::sdk::Uuid objectId =
            nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(QnUuid::createUuid());
        object->setId(objectId);
        object->setTypeId(m_objectTypeId);

        object->setBoundingBox(IObject::Rect(rect.x, rect.y, rect.w, rect.h));
        objectPacket->addItem(object);
    }
    metadataPackets->push_back(objectPacket);

    NX_OUTPUT << __func__ << "(): Created objects packet with " << rects.size() << " rects";
    return true;
}

bool DeviceAgent::makeMetadataPacketsFromRectsPostprocPed(
    std::vector<IMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    // TODO: #dmishin: STUB
    return makeMetadataPacketsFromRectsPostprocNone(metadataPackets, rects, ptsUs);
}

bool DeviceAgent::makeMetadataPacketsFromRectsPostprocCar(
    std::vector<IMetadataPacket*>* metadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs) const
{
    auto objectPacket = new ObjectMetadataPacket();
    objectPacket->setTimestampUs(ptsUs);
    objectPacket->setDurationUs(0);

    m_tracker.filterAndTrack(metadataPackets, rects, ptsUs);

    return true;
}

int64_t DeviceAgent::usSinceEpoch() const
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
}

void DeviceAgent::setTrackerAttributeOptions(
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
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
