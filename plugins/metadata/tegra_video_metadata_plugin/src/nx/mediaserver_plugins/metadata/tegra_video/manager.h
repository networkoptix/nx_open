#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <nx/sdk/metadata/simple_manager.h>

#include <tegra_video.h> //< libtegra_video.so - analytics lib for Tx1 and Tx2.

#include "plugin.h"

#include <nx/mediaserver_plugins/metadata/tegra_video/naive_object_tracker.h>
#include <nx/mediaserver_plugins/metadata/tegra_video/naive_detection_smoother.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

class Manager: public nx::sdk::metadata::SimpleManager
{
public:
    Manager(Plugin* plugin);
    virtual ~Manager() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

protected:
    virtual bool pushVideoFrame(
        const nx::sdk::metadata::CommonCompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets) override;

private:
    nx::sdk::Error stopFetchingMetadataThreadUnsafe();

    bool pushCompressedFrame(
        const nx::sdk::metadata::CommonCompressedVideoPacket* videoPacket);

    bool pullRectsForFrame(std::vector<TegraVideo::Rect>* rects, int64_t* outPtsUs);

    bool makeMetadataPacketsFromRects(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocNone(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocPed(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    bool makeMetadataPacketsFromRectsPostprocCar(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs) const;

    int64_t usSinceEpoch() const;

    void setTrackerAttributeOptions(
        const std::map<QString, std::vector<QString>>& options);

private:
    Plugin* const m_plugin;
    int m_counter = 0;
    int m_counterObjects = 0;
    nxpl::NX_GUID m_eventTypeId;
    nxpl::NX_GUID m_objectTypeId;

    std::unique_ptr<TegraVideo, decltype(&tegraVideoDestroy)> m_tegraVideo{
        nullptr, tegraVideoDestroy};

    mutable NaiveObjectTracker m_tracker;
    int64_t m_lastFramePtsUs;
};

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
