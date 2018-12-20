#include "openalpr_pipeline.h"

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>

#define NX_PRINT_PREFIX "deepstream::OpenAlprPipeline::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

OpenAlprPipeline::OpenAlprPipeline(const gstreamer::ElementName& pipelineName, Engine* engine):
    base_type(pipelineName, engine)
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
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
