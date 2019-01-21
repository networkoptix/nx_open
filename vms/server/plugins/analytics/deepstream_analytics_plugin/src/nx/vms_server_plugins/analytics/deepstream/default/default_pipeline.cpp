#include "default_pipeline.h"

#define NX_PRINT_PREFIX "deepstream::DefaultPipeline::"
#include <nx/kit/debug.h>

#include <nx/sdk/analytics/i_data_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

namespace {

static const int kDefaultTrackingLifetime = 1;

} // namespace

DefaultPipeline::DefaultPipeline(
    const nx::gstreamer::ElementName& pipelineName,
    Engine* engine)
    :
    base_type(pipelineName, engine),
    m_trackingMapper(kDefaultTrackingLifetime),
    m_objectClassDescriptions(m_engine->objectClassDescritions())
{
    NX_OUTPUT << __func__ << " Creating DefaultPipeline...";
}

TrackingMapper* DefaultPipeline::trackingMapper()
{
    return &m_trackingMapper;
}

const std::vector<ObjectClassDescription>& DefaultPipeline::objectClassDescriptions() const
{
    return m_objectClassDescriptions;
}

} // namespace deepstream
} // namespace analytics
} // namespace mediserver_plugins
} // namespace nx
