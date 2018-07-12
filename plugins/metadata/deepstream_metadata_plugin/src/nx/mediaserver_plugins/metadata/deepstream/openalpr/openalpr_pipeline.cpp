#include "openalpr_pipeline.h"

#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_metadata_plugin_ini.h>

#define NX_PRINT_PREFIX "deepstream::OpenAlprPipeline::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

OpenAlprPipeline::OpenAlprPipeline(const gstreamer::ElementName& pipelineName, Plugin* plugin):
    base_type(pipelineName, plugin)
{
    NX_OUTPUT << __func__ << " Creating OpenAlprPipeline...";
}

SimpleLicensePlateTracker* OpenAlprPipeline::licensePlateTracker()
{
    return &m_licensePlateTracker;
}

bool OpenAlprPipeline::shouldDropFrame(int64_t frameTimestamp) const
{
    return m_lastFrameTimestampUs - frameTimestamp > ini().maxAllowedFrameDelayMs * 1000;
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
