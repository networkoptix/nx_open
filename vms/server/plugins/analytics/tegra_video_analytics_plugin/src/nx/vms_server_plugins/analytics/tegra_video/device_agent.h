#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <string>

#include <nx/sdk/analytics/helpers/video_frame_processing_device_agent.h>

#include <tegra_video.h> //< libtegra_video.so - analytics for Tegra; the lib is a stub on a PC.

#include "engine.h"

#include <nx/vms_server_plugins/analytics/tegra_video/naive_object_tracker.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class DeviceAgent: public nx::sdk::analytics::VideoFrameProcessingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

protected:
    virtual std::string manifest() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets) override;

private:
    bool pushCompressedFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoPacket);

    bool pullRectsForFrame(std::vector<TegraVideo::Rect>* rects, int64_t* outPtsUs);

    bool makeMetadataPacketsFromRects(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocNone(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocPed(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocCar(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    int64_t usSinceEpoch() const;

    void setTrackerAttributeOptions(
        const std::map<std::string, std::vector<std::string>>& options);

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
} // namespace vms_server_plugins
} // namespace nx
