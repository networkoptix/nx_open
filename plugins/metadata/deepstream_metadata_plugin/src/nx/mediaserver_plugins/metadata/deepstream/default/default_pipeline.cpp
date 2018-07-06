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
    const nx::gstreamer::ElementName& pipelineName,
    deepstream::Plugin* plugin)
    :
    base_type(pipelineName, plugin),
    m_trackingMapper(kDefaultTrackingLifetime),
    m_objectClassDescriptions(m_plugin->objectClassDescritions())
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
} // namespace metadata
} // namespace mediserver_plugins
} // namespace nx
