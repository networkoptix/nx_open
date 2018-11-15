#pragma once

#include <nx/mediaserver_plugins/analytics/deepstream/base_pipeline_builder.h>

#include <nx/mediaserver_plugins/analytics/deepstream/openalpr/openalpr_pipeline.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace deepstream {

class OpenAlprPipelineBuilder: public BasePipelineBuilder
{
    using base_type = BasePipelineBuilder;

public:
    OpenAlprPipelineBuilder(
        nx::mediaserver_plugins::analytics::deepstream::Engine* engine);

    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipelineName) override;

private:
    std::unique_ptr<nx::gstreamer::Bin> buildOpenAlprBin(
        OpenAlprPipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);


};

} // namespace deepstream
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
