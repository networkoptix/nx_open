#include "base_pipeline_builder.h"
#include "base_callbacks.h"
#include "deepstream_common.h"
#include "deepstream_analytics_plugin_ini.h"

#include "utils.h"

#define NX_PRINT_PREFIX "deepstream::BasePipelineBuilder::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

BasePipelineBuilder::BasePipelineBuilder(Engine* engine):
    m_engine(engine)
{
}

std::unique_ptr<gstreamer::Element> BasePipelineBuilder::createAppSource(
    gstreamer::Pipeline* pipeline,
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating application source for pipeline " << pipelineName;
    auto appSource = std::make_unique<nx::gstreamer::Element>(
        kGstElementAppSrc,
        makeElementName(pipelineName, kGstElementAppSrc, "appSource"));

#if 0
    // TODO: #dmishin fix initializing GValue from instance
    appSource->setProperties(
        {nx::gstreamer::makeProperty("caps", gst_caps_new_simple("video/x-h264", NULL))});
#endif

    // TODO: #dmishin pass codec somehow or consider don't set caps at all
    g_object_set(
        G_OBJECT(appSource->nativeElement()),
        "caps",
        gst_caps_new_simple(
            "video/x-h264",
            NULL,
            NULL),
        "is-live",
        TRUE,
        NULL);

    NX_OUTPUT
        << __func__
        << " Connecting appsource 'need-data' signal for pipeline " << pipelineName;

    g_signal_connect(
        appSource->nativeElement(),
        "need-data",
        G_CALLBACK(appSourceNeedData),
        pipeline);

    return appSource;
}

std::unique_ptr<gstreamer::Element> BasePipelineBuilder::buildPreGieBin(const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating decode bin for " << pipelineName;
    auto decodeBin = std::make_unique<nx::gstreamer::Element>(
        kGstElementDecodeBin,
        makeElementName(pipelineName, kGstElementDecodeBin, "mainDecodeBin"));

    return decodeBin;
}

std::unique_ptr<gstreamer::Bin> BasePipelineBuilder::buildPrimaryGieBin(
    const gstreamer::ElementName& pipelineName,
    BasePipeline* /*pipeline*/)
{
    NX_OUTPUT << __func__ << " Creating primary GIE bin for " << pipelineName;
    auto inputQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "pgieInputQueue"));

    auto converter = std::make_unique<nx::gstreamer::Element>(
        kNvidiaElementVideoConverter,
        makeElementName(pipelineName, kNvidiaElementVideoConverter, "pgieNvVideoConverter"));

    auto capsFilter = std::make_unique<nx::gstreamer::Element>(
        kGstElementCapsFilter,
        makeElementName(pipelineName, kGstElementCapsFilter, "pgieCapsFilter"));

    auto primaryGie = std::make_unique<nx::gstreamer::Element>(
        kNvidiaElementGie,
        makeElementName(pipelineName, kNvidiaElementGie, "pgieInferenceEngine"));

    auto outputQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "pgieOutputQueue"));

    auto bin = std::make_unique<nx::gstreamer::Bin>(
        makeElementName(pipelineName, "bin", "pgieBin"));

    bin->add(inputQueue.get());
    bin->add(converter.get());
    bin->add(capsFilter.get());
    bin->add(primaryGie.get());
    bin->add(outputQueue.get());

    converter->linkAfter(inputQueue.get());
    capsFilter->linkAfter(converter.get());
    primaryGie->linkAfter(capsFilter.get());
    outputQueue->linkAfter(primaryGie.get());

    primaryGie->setProperties(
        {
            nx::gstreamer::makeProperty("net-scale-factor", ini().pgie_netScaleFactor),
            nx::gstreamer::makeProperty("model-path", ini().pgie_modelFile),
            nx::gstreamer::makeProperty("protofile-path", ini().pgie_protoFile),
            nx::gstreamer::makeProperty("net-stride", (guint) ini().pgie_netStride),
            nx::gstreamer::makeProperty("batch-size", (guint) ini().pgie_batchSize),
            nx::gstreamer::makeProperty("gie-mode", (guint) 1),
            nx::gstreamer::makeProperty("interval", (guint) ini().pgie_inferenceInterval),
            nx::gstreamer::makeProperty("roi-top-offset", ini().pgie_roiTopOffset),
            nx::gstreamer::makeProperty("roi-bottom-offset", ini().pgie_roiBottomOffset),
            nx::gstreamer::makeProperty("detected-min-w-h", ini().pgie_minDetectedWH),
            nx::gstreamer::makeProperty("detected-max-w-h", ini().pgie_maxDetectedWH),
            nx::gstreamer::makeProperty("model-cache", ini().pgie_cacheFile),
            nx::gstreamer::makeProperty("model-color-format", ini().pgie_colorFormat),
            nx::gstreamer::makeProperty("class-thresh-params", ini().pgie_classThresholds),
            nx::gstreamer::makeProperty("detect-clr", guint{0}),
            nx::gstreamer::makeProperty("force-fp32", ini().pgie_forceFp32),
            nx::gstreamer::makeProperty("enable-dbscan", ini().pgie_enableDbScan),
            nx::gstreamer::makeProperty("crypto-flags", gboolean{0}),
            nx::gstreamer::makeProperty(
                "sec-class-threshold",
                ini().pgie_secondaryGieClassifierThreshold),
            nx::gstreamer::makeProperty("parse-func", guint{4}),
            nx::gstreamer::makeProperty("gie-unique-id", guint{1}),
            nx::gstreamer::makeProperty("is-classifier", ini().pgie_isClassifier),
            nx::gstreamer::makeProperty("offsets", (const gchar*)NULL/*ini().pgie_colorOffsets*/),
            nx::gstreamer::makeProperty("output-bbox-layer-name", ini().pgie_outputBboxLayerName),
            nx::gstreamer::makeProperty(
                "output-coverage-layer-names",
                ini().pgie_outputCoverageLayerNames),
        });

    bin->createGhostPad(inputQueue.get(), kSinkPadName);
    bin->createGhostPad(outputQueue.get(), kSourcePadName);

    return bin;
}

std::unique_ptr<gstreamer::Element> BasePipelineBuilder::createFakeSink(
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating fake sink for pipeline " << pipelineName;
    auto fakeSink = std::make_unique<nx::gstreamer::Element>(
        kGstElementFakeSink,
        makeElementName(pipelineName, kGstElementFakeSink, "fakeSink"));

    return fakeSink;
}

void BasePipelineBuilder::createMainLoop(
    BasePipeline* pipeline,
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating main loop and bus for pipeline " << pipelineName;
    auto mainLoop = LoopPtr(
        g_main_loop_new(NULL, false),
        [](GMainLoop* loop) { g_main_loop_unref(loop); });

    auto bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline->nativeElement()));
    gst_bus_add_watch(bus, busCallback, pipeline);
    gst_object_unref(bus);

    pipeline->setMainLoop(std::move(mainLoop));

    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(pipeline->nativeElement()),
        GST_DEBUG_GRAPH_SHOW_ALL,
        "nx-pipeline-null-initial");
}

void BasePipelineBuilder::connectOnPadAdded(
    gstreamer::Element* connectSource,
    gstreamer::Element* connectSink,
    const gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT
        << __func__
        << " Connecting decoder bin 'pad-added' signal for pipeline " << pipelineName;

    auto pad = gst_element_get_static_pad(
        connectSink->nativeElement(),
        "sink");

    g_signal_connect(
        connectSource->nativeElement(),
        "pad-added",
        G_CALLBACK(connectPads),
        pad);

    g_object_unref(G_OBJECT(pad));
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
