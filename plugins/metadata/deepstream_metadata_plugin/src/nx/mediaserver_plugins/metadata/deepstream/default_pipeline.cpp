#include "default_pipeline.h"

#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_metadata_plugin_ini.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/data_packet.h>
#include <nx/sdk/metadata/compressed_video_packet.h>
#define NX_PRINT_PREFIX "deepstream::DefaultPipeline::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

namespace {

static const int kDefaultTrackingLifetime = 1;

} // namespace

DefaultPipeline::DefaultPipeline(
    const nx::gstreamer::ElementName& elementName,
    nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin)
    :
    base_type(elementName),
    m_plugin(plugin),
    m_trackingMapper(kDefaultTrackingLifetime),
    m_objectClassDescriptions(m_plugin->objectClassDescritions())
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
    if (!dataPacket)
        return true;

    std::lock_guard<std::mutex> guard(m_mutex);
    NX_OUTPUT << __func__ << " Pushing data packet! Queue size is: " << m_packetQueue.size();

    nxpt::ScopedRef<nx::sdk::metadata::CompressedVideoPacket> video(
        dataPacket->queryInterface(nx::sdk::metadata::IID_CompressedVideoPacket));

    if (video)
    {
        m_lastFrameTimestampUs = video->timestampUsec();
        m_currentFrameWidth = video->width();
        m_currentFrameHeight = video->height();
    }

    m_packetQueue.push(dataPacket);
    dataPacket->addRef();
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

sdk::metadata::DataPacket* DefaultPipeline::nextDataPacket()
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

GMainLoop* DefaultPipeline::mainLoop()
{
    return m_mainLoop.get();
}

TrackingMapper* DefaultPipeline::trackingMapper()
{
    return &m_trackingMapper;
}

SimpleLicensePlateTracker* DefaultPipeline::licensePlateTracker()
{
    return &m_licensePlateTracker;
}

bool DefaultPipeline::shouldDropFrame(int64_t frameTimestamp) const
{
    return m_lastFrameTimestampUs - frameTimestamp > ini().maxAllowedFrameDelayMs * 1000;
}

void DefaultPipeline::setMainLoop(LoopPtr loop)
{
    NX_OUTPUT << __func__ << " Setting main loop";
    m_mainLoop = std::move(loop);
}

int DefaultPipeline::currentFrameWidth() const
{
    return m_currentFrameWidth;
}

int DefaultPipeline::currentFrameHeight() const
{
    return m_currentFrameHeight;
}

const std::vector<ObjectClassDescription>& DefaultPipeline::objectClassDescriptions() const
{
    return m_objectClassDescriptions;
}

std::chrono::microseconds DefaultPipeline::currentTimeUs() const
{
    return m_plugin->currentTimeUs();
}

} // namespace deepstream
} // namespace metadata
} // namespace mediserver_plugins
} // namespace nx
