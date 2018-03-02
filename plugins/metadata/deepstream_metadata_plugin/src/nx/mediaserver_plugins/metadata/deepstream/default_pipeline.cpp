#include "default_pipeline.h"

#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_metadata_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::DefaultPipeline::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

namespace {

static const int kDefaultTrackingLifetime = 1;

} // namespace

DefaultPipeline::DefaultPipeline(const nx::gstreamer::ElementName& elementName):
    base_type(elementName),
    m_trackingMapper(kDefaultTrackingLifetime)
{
    NX_OUTPUT << __func__ << " Creating DefaultPipeline...";
}

void DefaultPipeline::setMetadataCallback(nx::gstreamer::MetadataCallback metadataCallback)
{
    NX_OUTPUT << __func__ << " Setting metadata callback";
    m_metadataCallback = std::move(metadataCallback);
}

bool DefaultPipeline::pushDataPacket(nx::sdk::metadata::DataPacket* dataPacket)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    NX_OUTPUT << __func__ << " Pushing data packet! Queue size is: " << m_packetQueue.size();
    m_packetQueue.push(dataPacket);
    m_wait.notify_all();

    return true;
}

bool DefaultPipeline::start()
{
    auto result = gst_element_set_state(nativeElement(), GST_STATE_PLAYING);
    NX_OUTPUT << __func__ << " Starting pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

bool DefaultPipeline::stop()
{
    auto result = gst_element_set_state(nativeElement(), GST_STATE_NULL);
    NX_OUTPUT << __func__ << " Stopping pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

bool DefaultPipeline::pause()
{
    auto result = gst_element_set_state(nativeElement(), GST_STATE_PAUSED);
    NX_OUTPUT << __func__ << " Pausing pipeline, result: " << result;
    return result == GST_STATE_CHANGE_SUCCESS;
}

GstState DefaultPipeline::state() const
{
    return GST_STATE(nativeElement());
}

sdk::metadata::DataPacket*DefaultPipeline::nextDataPacket()
{
    NX_OUTPUT << __func__ << " Fetching next data packet...";
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_packetQueue.empty())
        m_wait.wait(lock);

    NX_OUTPUT
        << __func__
        << " There are some packets in the queue, queue size is: " << m_packetQueue.size();

    auto packet = m_packetQueue.front();
    m_packetQueue.pop();

    return packet;
}

void DefaultPipeline::handleMetadata(sdk::metadata::MetadataPacket* packet)
{
    NX_OUTPUT << __func__ << " Calling metadata handler";
    m_metadataCallback(packet);
    packet->releaseRef();
}

GMainLoop*DefaultPipeline::mainLoop()
{
    return m_mainLoop.get();
}

TrackingMapper*DefaultPipeline::trackingMapper()
{
    return &m_trackingMapper;
}

void DefaultPipeline::setMainLoop(LoopPtr loop)
{
    NX_OUTPUT << __func__ << " Setting main loop";
    m_mainLoop = std::move(loop);
}

} // namespace deepstream
} // namespace metadata
} // namespace mediserver_plugins
} // namespace nx
