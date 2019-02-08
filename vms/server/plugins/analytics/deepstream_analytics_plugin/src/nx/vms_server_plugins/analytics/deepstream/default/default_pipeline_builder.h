#pragma once

#include <nx/vms_server_plugins/analytics/deepstream/engine.h>
#include <nx/vms_server_plugins/analytics/deepstream/base_pipeline_builder.h>

#include <nx/vms_server_plugins/analytics/deepstream/default/tracking_mapper.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/default_pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/object_class_description.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class DefaultPipeline;

using RawLabels = std::vector<std::vector<std::string>>;

class DefaultPipelineBuilder: public BasePipelineBuilder
{
    using base_type = BasePipelineBuilder;

public:
    DefaultPipelineBuilder(
        nx::vms_server_plugins::analytics::deepstream::Engine* engine);

    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipeleineName) override;

protected:
    std::unique_ptr<nx::gstreamer::Bin> buildTrackerBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Bin> buildSecondaryGieBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);

    std::map<LabelMappingId, LabelMapping> makeLabelMapping();

    RawLabels parseLabelFile(const std::string& path) const;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
