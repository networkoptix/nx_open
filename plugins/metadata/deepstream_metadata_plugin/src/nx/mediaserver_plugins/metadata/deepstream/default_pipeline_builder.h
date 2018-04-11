#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default_pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/object_class_description.h>
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
    DefaultPipelineBuilder(
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipeleineName) override;

private:
    std::unique_ptr<nx::gstreamer::Pipeline> buildDefaultDeepStreamPipeline(
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Pipeline> buildOpenAlprDeepStreamPipeline(
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Element> buildPreGieBin(
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Bin> buildPrimaryGieBin(
        const nx::gstreamer::ElementName& pipeleinName,
        DefaultPipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Bin> buildTrackerBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Bin> buildOpenAlprBin(
        DefaultPipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineNAme);

    std::unique_ptr<nx::gstreamer::Bin> buildSecondaryGieBin(
        const nx::gstreamer::ElementName& pipelineName,
        DefaultPipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Element> createAppSource(
        nx::gstreamer::Pipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Element> createFakeSink(
        const nx::gstreamer::ElementName& pipelineName);

    void connectToPreGieBin(
        nx::gstreamer::Element* preGieBin,
        nx::gstreamer::Element* postGieElement,
        const nx::gstreamer::ElementName& pipelineName);

    void createMainLoop(
        DefaultPipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);

    std::map<LabelMappingId, LabelMapping> makeLabelMapping();

    RawLabels parseLabelFile(const std::string& path) const;

private:
    nx::mediaserver_plugins::metadata::deepstream::Plugin* m_plugin;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
