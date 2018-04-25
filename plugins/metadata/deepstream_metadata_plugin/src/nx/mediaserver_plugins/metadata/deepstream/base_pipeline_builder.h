#pragma once

#include <nx/gstreamer/element.h>
#include <nx/gstreamer/gstreamer_common.h>

#include <nx/mediaserver_plugins/metadata/deepstream/base_pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>
#include <nx/mediaserver_plugins/metadata/deepstream/pipeline_builder.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class BasePipelineBuilder: public PipelineBuilder
{
public:
    BasePipelineBuilder(deepstream::Plugin* plugin);

    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(
        const nx::gstreamer::ElementName& pipelineName) = 0;

protected:
    std::unique_ptr<nx::gstreamer::Element> createAppSource(
        nx::gstreamer::Pipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Element> buildPreGieBin(
        const nx::gstreamer::ElementName& pipelineName);

    std::unique_ptr<nx::gstreamer::Bin> buildPrimaryGieBin(
        const nx::gstreamer::ElementName& pipelineName,
        BasePipeline* pipeline);

    std::unique_ptr<nx::gstreamer::Element> createFakeSink(
        const nx::gstreamer::ElementName& pipelineName);

    void createMainLoop(
        BasePipeline* pipeline,
        const nx::gstreamer::ElementName& pipelineName);

    void connectOnPadAdded(
        nx::gstreamer::Element* connectSource,
        nx::gstreamer::Element* connectSink,
        const nx::gstreamer::ElementName& pipelineName);

protected:
    deepstream::Plugin* m_plugin;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
