#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/base_pipeline_builder.h>

#include <nx/mediaserver_plugins/metadata/deepstream/openalpr/openalpr_pipeline.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class OpenAlprPipelineBuilder: public BasePipelineBuilder
{
    using base_type = BasePipelineBuilder;

public:
    OpenAlprPipelineBuilder(
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipelineName) override;

private:
    std::unique_ptr<nx::gstreamer::Bin> buildOpenAlprBin(
        OpenAlprPipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);


};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
