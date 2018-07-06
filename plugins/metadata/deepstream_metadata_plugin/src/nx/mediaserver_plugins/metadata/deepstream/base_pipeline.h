#pragma once

#include <queue>
#include <chrono>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <nx/sdk/metadata/data_packet.h>
#include <nx/gstreamer/pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

using LoopPtr = std::unique_ptr<GMainLoop, std::function<void(GMainLoop*)>>;

class BasePipeline: public nx::gstreamer::Pipeline
{
    using base_type = nx::gstreamer::Pipeline;

public:
    BasePipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

    virtual void setMetadataCallback(nx::gstreamer::MetadataCallback metadataCallback) override;

    virtual bool pushDataPacket(nx::sdk::metadata::DataPacket* dataPacket) override;

    virtual nx::sdk::metadata::DataPacket* nextDataPacket();

    virtual bool start() override;

    virtual bool stop() override;

    virtual bool pause() override;

    virtual GstState state() const override;

    virtual void handleMetadata(nx::sdk::metadata::MetadataPacket* packet);

    virtual GMainLoop* mainLoop();

    virtual void setMainLoop(LoopPtr loop);

    virtual int currentFrameWidth() const;

    virtual int currentFrameHeight() const;

    virtual std::chrono::microseconds currentTimeUs() const;

protected:
    deepstream::Plugin* m_plugin;
    nx::gstreamer::MetadataCallback m_metadataCallback;
    std::queue<nx::sdk::metadata::DataPacket*> m_packetQueue;
    LoopPtr m_mainLoop;

    int m_currentFrameWidth = 0;
    int m_currentFrameHeight = 0;
    int64_t m_lastFrameTimestampUs = 0;

    mutable std::condition_variable m_wait;
    mutable std::mutex m_mutex;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
