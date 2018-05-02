#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>
#include <nx/mediaserver_plugins/metadata/deepstream/base_pipeline_builder.h>

#include <nx/mediaserver_plugins/metadata/deepstream/default/tracking_mapper.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/default_pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/object_class_description.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class DefaultPipeline;

using RawLabels = std::vector<std::vector<std::string>>;

class DefaultPipelineBuilder: public BasePipelineBuilder
{
    using base_type = BasePipelineBuilder;

public:
    DefaultPipelineBuilder(
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
