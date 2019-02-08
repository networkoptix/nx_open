#pragma once

#include <nx/gstreamer/element.h>
#include <nx/gstreamer/gstreamer_common.h>

#include <nx/vms_server_plugins/analytics/deepstream/base_pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/engine.h>
#include <nx/vms_server_plugins/analytics/deepstream/pipeline_builder.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class BasePipelineBuilder: public PipelineBuilder
{
public:
    BasePipelineBuilder(Engine* engine);

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
    Engine* m_engine;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
