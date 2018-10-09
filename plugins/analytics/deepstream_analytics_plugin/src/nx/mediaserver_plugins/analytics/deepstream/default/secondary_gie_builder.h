#pragma once

#include <nx/gstreamer/bin.h>
#include <nx/mediaserver_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/mediaserver_plugins/analytics/deepstream/default/default_pipeline.h>

namespace nx {
namespace mediaserver_plugins {
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
} // namespace mediaserver_plugins
} // namespace nx
