#pragma once

#include <queue>
#include <chrono>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/gstreamer/pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/engine.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

using LoopPtr = std::unique_ptr<GMainLoop, std::function<void(GMainLoop*)>>;

class BasePipeline: public nx::gstreamer::Pipeline
{
    using base_type = nx::gstreamer::Pipeline;

public:
    BasePipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::vms_server_plugins::analytics::deepstream::Engine* engine);

    virtual void setMetadataCallback(nx::gstreamer::MetadataCallback metadataCallback) override;

    virtual bool pushDataPacket(nx::sdk::analytics::IDataPacket* dataPacket) override;

    virtual nx::sdk::analytics::IDataPacket* nextDataPacket();

    virtual bool start() override;

    virtual bool stop() override;

    virtual bool pause() override;

    virtual GstState state() const override;

    virtual void handleMetadata(nx::sdk::analytics::IMetadataPacket* packet);

    virtual GMainLoop* mainLoop();

    virtual void setMainLoop(LoopPtr loop);

    virtual int currentFrameWidth() const;

    virtual int currentFrameHeight() const;

    virtual std::chrono::microseconds currentTimeUs() const;

protected:
    Engine* m_engine = nullptr;
    nx::gstreamer::MetadataCallback m_metadataCallback;
    std::queue<nx::sdk::analytics::IDataPacket*> m_packetQueue;
    LoopPtr m_mainLoop;

    int m_currentFrameWidth = 0;
    int m_currentFrameHeight = 0;
    int64_t m_lastFrameTimestampUs = 0;

    mutable std::condition_variable m_wait;
    mutable std::mutex m_mutex;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
