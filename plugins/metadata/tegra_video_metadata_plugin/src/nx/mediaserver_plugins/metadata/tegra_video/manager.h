#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>

#include <tegra_video.h> //< libtegra_video.so - analytics lib for Tx1 and Tx2.

#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

class Manager:
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractConsumingMetadataManager>
{
public:
    Manager(Plugin* plugin);
    virtual ~Manager();

    virtual Plugin* plugin() override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata() override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::AbstractMetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(
        nx::sdk::Error* error) const override;

    virtual nx::sdk::Error putData(nx::sdk::metadata::AbstractDataPacket* dataPacket) override;

private:
    nx::sdk::Error stopFetchingMetadataThreadUnsafe();

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

    bool pushFrameAndGetMetadataPackets(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* metadataPackets,
        nx::sdk::metadata::AbstractDataPacket* mediaPacket) const;

    int64_t usSinceEpoch() const;

private:
    Plugin* const m_plugin;
    mutable std::mutex m_mutex;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
    int m_counter = 0;
    int m_counterObjects = 0;
    nxpl::NX_GUID m_eventTypeId;
    nxpl::NX_GUID m_objectTypeId;

    std::unique_ptr<TegraVideo> m_tegraVideo;
};

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
