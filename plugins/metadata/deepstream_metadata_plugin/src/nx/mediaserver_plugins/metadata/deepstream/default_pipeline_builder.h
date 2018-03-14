#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/pipeline_builder.h>
#include <nx/mediaserver_plugins/metadata/deepstream/tracking_mapper.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class DefaultPipeline;

using RawLabels = std::vector<std::vector<std::string>>;

class DefaultPipelineBuilder: public PipelineBuilder
{

public:
    DefaultPipelineBuilder();
    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipeleineName) override;

private:
    std::unique_ptr<nx::gstreamer::Element> buildPreGieBin(
        const nx::gstreamer::ElementName& pipelineName);
    std::unique_ptr<nx::gstreamer::Bin> buildPrimaryGieBin(
        const nx::gstreamer::ElementName& pipeleinName,
        DefaultPipeline* pipeline);
    std::unique_ptr<nx::gstreamer::Bin> buildTrackerBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);
    std::unique_ptr<nx::gstreamer::Bin> buildSecondaryGieBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Element> createAppSource(
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Element> createFakeSink(
        const nx::gstreamer::ElementName& pipelineName);

    std::map<LabelMappingId, LabelMapping> makeLabelMapping();

    RawLabels parseLabelFile(const std::string& path) const;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
