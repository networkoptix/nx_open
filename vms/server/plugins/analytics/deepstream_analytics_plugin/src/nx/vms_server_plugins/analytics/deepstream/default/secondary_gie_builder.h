#pragma once

#include <nx/gstreamer/bin.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/default_pipeline.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class SecondaryGieBuilder
{
public:
    static std::unique_ptr<nx::gstreamer::Bin> buildSecondaryGie(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
