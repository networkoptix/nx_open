#include "base_pipeline.h"

#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::BasePipeline::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

BasePipeline::BasePipeline(const gstreamer::ElementName& pipelineName, Engine* engine):
    base_type(pipelineName),
    m_engine(engine)
{
    NX_OUTPUT << __func__ << " BasePipeline constructor";
}

void BasePipeline::setMetadataCallback(nx::gstreamer::MetadataCallback metadataCallback)
{
    NX_OUTPUT << __func__ << " Setting metadata callback";
    m_metadataCallback = std::move(metadataCallback);
}

bool BasePipeline::pushDataPacket(nx::sdk::analytics::IDataPacket* dataPacket)
{
    if (!dataPacket)
        return true;

    std::lock_guard<std::mutex> guard(m_mutex);
    NX_OUTPUT << __func__ << " Pushing data packet! Queue size is: " << m_packetQueue.size();

    if (const auto video = nx::sdk::Ptr<nx::sdk::analytics::ICompressedVideoPacket>(
        dataPacket->queryInterface(nx::sdk::analytics::IID_CompressedVideoPacket)))
    {
        m_lastFrameTimestampUs = video->timestampUs();
        m_currentFrameWidth = video->width();
        m_currentFrameHeight = video->height();
    }

    m_packetQueue.push(dataPacket);
    dataPacket->addRef();
    m_wait.notify_all();

    return true;
}

nx::sdk::analytics::IDataPacket* BasePipeline::nextDataPacket()
{
    NX_OUTPUT << __func__ << " Fetching next data packet...";
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_packetQueue.empty())
        m_wait.wait(lock);

    NX_OUTPUT
        << __func__
        << " There are some packets in the queue, queue size is: " << m_packetQueue.size();

    auto packet = m_packetQueue.front();
    m_packetQueue.pop();

    return packet;
}

void BasePipeline::handleMetadata(nx::sdk::analytics::IMetadataPacket* packet)
{
    NX_OUTPUT << __func__ << " Calling metadata handler";
    m_metadataCallback(packet);
    packet->releaseRef();
}

bool BasePipeline::start()
{
    auto result = gst_element_set_state(nativeElement(), GST_STATE_PLAYING);
    NX_OUTPUT << __func__ << " Starting pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

bool BasePipeline::stop()
{
    // TODO: #dmishin fix this method.
    auto result = gst_element_set_state(nativeElement(), GST_STATE_NULL);
    NX_OUTPUT << __func__ << " Stopping pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

bool BasePipeline::pause()
{
    // TODO: #dmishin fix this method.
    auto result = gst_element_set_state(nativeElement(), GST_STATE_PAUSED);
    NX_OUTPUT << __func__ << " Pausing pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

GstState BasePipeline::state() const
{
    return GST_STATE(nativeElement());
}

GMainLoop* BasePipeline::mainLoop()
{
    return m_mainLoop.get();
}

void BasePipeline::setMainLoop(LoopPtr loop)
{
    NX_OUTPUT << __func__ << " Setting main loop";
    m_mainLoop = std::move(loop);
}

int BasePipeline::currentFrameWidth() const
{
    return m_currentFrameWidth;
}

int BasePipeline::currentFrameHeight() const
{
    return m_currentFrameHeight;
}

std::chrono::microseconds BasePipeline::currentTimeUs() const
{
    return m_engine->currentTimeUs();
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
