#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <nx/sdk/analytics/common_video_frame_processing_device_agent.h>

#include <tegra_video.h> //< libtegra_video.so - analytics for Tegra; the lib is a stub on a PC.

#include "engine.h"

#include <nx/mediaserver_plugins/analytics/tegra_video/naive_object_tracker.h>
#include <nx/sdk/analytics/compressed_video_packet.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace tegra_video {

class DeviceAgent: public nx::sdk::analytics::CommonVideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifest() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::CompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets) override;

private:
    bool pushCompressedFrame(
        const nx::sdk::analytics::CompressedVideoPacket* videoPacket);

    bool pullRectsForFrame(std::vector<TegraVideo::Rect>* rects, int64_t* outPtsUs);

    bool makeMetadataPacketsFromRects(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocNone(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocPed(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocCar(
        std::vector<nx::sdk::analytics::MetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    int64_t usSinceEpoch() const;

    void setTrackerAttributeOptions(
        const std::map<QString, std::vector<QString>>& options);

private:
    int m_counter = 0;
    int m_counterObjects = 0;
    std::string m_eventTypeId;
    std::string m_objectTypeId;

    std::unique_ptr<TegraVideo, decltype(&tegraVideoDestroy)> m_tegraVideo{
        nullptr, tegraVideoDestroy};

    mutable NaiveObjectTracker m_tracker;
    int64_t m_lastFramePtsUs;
};

} // namespace tegra_video
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
