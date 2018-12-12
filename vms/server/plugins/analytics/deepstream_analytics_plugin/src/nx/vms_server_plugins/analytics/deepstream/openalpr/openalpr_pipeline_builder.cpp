#include "openalpr_pipeline_builder.h"

#include <nx/vms_server_plugins/analytics/deepstream/utils.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/openalpr/openalpr_callbacks.h>

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::OpenAlprPipelineBuilder::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

OpenAlprPipelineBuilder::OpenAlprPipelineBuilder(Engine* engine):
    base_type(engine)
{
}

std::unique_ptr<gstreamer::Pipeline> OpenAlprPipelineBuilder::build(
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " (OpenALPR) Building pipeline " << pipelineName;
    auto pipeline =
        std::make_unique<OpenAlprPipeline>(pipelineName, m_engine);

    NX_OUTPUT << __func__ << " (OpenALPR) Creating elements and bins for " << pipelineName;
    auto appSource = createAppSource(pipeline.get(), pipelineName);
    auto preGieBin = buildPreGieBin(pipelineName);
    auto primaryGieBin = buildPrimaryGieBin(pipelineName, pipeline.get());
    auto openAlprBin = buildOpenAlprBin(pipeline.get(), pipelineName);
#if 0
    auto tracker = buildTrackerBin(pipelineName, pipeline.get());
#endif
    auto fakeSink = createFakeSink(pipelineName);

    NX_OUTPUT << __func__ << " (OpenALPR) Adding elements and bins to pipeline " << pipelineName;
    pipeline->add(appSource.get());
    pipeline->add(preGieBin.get());
    pipeline->add(primaryGieBin.get());
    pipeline->add(openAlprBin.get());
#if 0
    pipeline->add(tracker.get());
#endif
    pipeline->add(fakeSink.get());

    preGieBin->linkAfter(appSource.get());
    openAlprBin->linkAfter(primaryGieBin.get());
#if 0
    tracker->linkAfter(openAlprBin.get());
#endif
    fakeSink->linkAfter(openAlprBin.get());

    connectOnPadAdded(
        preGieBin.get(),
        primaryGieBin.get(),
        pipelineName);

    createMainLoop(pipeline.get(), pipelineName);

    //-- HANDLE METADATA
    openAlprBin
        ->pad(kSourcePadName)
        ->addProbe(processOpenAlprResult, GST_PAD_PROBE_TYPE_BUFFER, pipeline.get());

    return pipeline;
}

std::unique_ptr<gstreamer::Bin> OpenAlprPipelineBuilder::buildOpenAlprBin(
    OpenAlprPipeline* pipeline,
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating OpenALPR bin for " << pipelineName;

    auto openAlpr = std::make_unique<nx::gstreamer::Element>(
        kElementOpenAlpr,
        makeElementName(pipelineName, kElementOpenAlpr, "openAlpr"));

    auto inputQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "openAlpr_inputQueue"));

    auto converter = std::make_unique<nx::gstreamer::Element>(
        kNvidiaElementVideoConverter,
        makeElementName(pipelineName, kNvidiaElementVideoConverter, "openAlpr_NvVideoConverter"));

    auto capsFilter = std::make_unique<nx::gstreamer::Element>(
        kGstElementCapsFilter,
        makeElementName(pipelineName, kGstElementCapsFilter, "openAlpr_CapsFilter"));

    auto outputQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "openAlpr_outputQueue"));

    g_object_set(
        G_OBJECT(openAlpr->nativeElement()),
        "full-frame",
        TRUE,
    #if 0 //< TODO: #dmishin pass processing resolution to OpenALPR
        "processing-width",
        1920,
        "processing-height",
        1080,
    #endif
        NULL);

    auto bin = std::make_unique<nx::gstreamer::Bin>(
        makeElementName(pipelineName, "bin", "openAlprBin"));

#if 0
    bin->add(inputQueue.get());
    bin->add(converter.get());
    bin->add(capsFilter.get());
#endif
    bin->add(openAlpr.get());
#if 0
    bin->add(outputQueue.get());

    converter->linkAfter(inputQueue.get());
    capsFilter->linkAfter(converter.get());
    openAlpr->linkAfter(inputQueue.get());
#endif

#if 0
    outputQueue->linkAfter(openAlpr.get());
#endif

    bin->createGhostPad(openAlpr.get(), kSinkPadName);
    bin->createGhostPad(openAlpr.get(), kSourcePadName);

    bin->pad(kSinkPadName)
        ->addProbe(dropOpenAlprFrames, GST_PAD_PROBE_TYPE_BUFFER, pipeline);

    return bin;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
