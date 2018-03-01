#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

#include <nx/gstreamer/pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/tracking_mapper.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

using LoopPtr = std::unique_ptr<GMainLoop, std::function<void(GMainLoop*)>>;

class DefaultPipeline: public nx::gstreamer::Pipeline
{
    using base_type = nx::gstreamer::Pipeline;

public:
    DefaultPipeline(const nx::gstreamer::ElementName& elementName);

    virtual void setMetadataCallback(nx::gstreamer::MetadataCallback metadataCallback) override;

    virtual bool pushDataPacket(nx::sdk::metadata::DataPacket* dataPacket) override;

    virtual bool start() override;

    virtual bool stop() override;

    virtual bool pause() override;

    virtual GstState state() const override;

    nx::sdk::metadata::DataPacket* nextDataPacket();

    void handleMetadata(nx::sdk::metadata::MetadataPacket* packet);

    GMainLoop* mainLoop();

    TrackingMapper* trackingMapper();

    void setMainLoop(LoopPtr loop);

private:
    nx::gstreamer::MetadataCallback m_metadataCallback;
    std::queue<nx::sdk::metadata::DataPacket*> m_packetQueue;
    LoopPtr m_mainLoop;
    TrackingMapper m_trackingMapper;

    mutable std::condition_variable m_wait;
    mutable std::mutex m_mutex;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediserver_plugins
} // namespace nx
