#pragma once

#include <nx/gstreamer/bin.h>
#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_common.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/default_pipeline.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class SecondaryGieBuilder
{
public:
    static std::unique_ptr<nx::gstreamer::Bin> buildSecondaryGie(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
