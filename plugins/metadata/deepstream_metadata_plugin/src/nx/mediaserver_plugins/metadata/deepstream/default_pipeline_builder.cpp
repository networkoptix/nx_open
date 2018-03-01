#include "default_pipeline_builder.h"

#include <iostream>
#include <fstream>

#include "utils.h"
#include "default_pipeline.h"
#include "default_pipeline_callbacks.h"

#include "deepstream_common.h"
#include "deepstream_metadata_plugin_ini.h"
#define NX_PRINT_PREFIX "deepstream::DefaultPipelineBuilder::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

DefaultPipelineBuilder::DefaultPipelineBuilder()
{
    ini().reload();
}

std::unique_ptr<nx::gstreamer::Pipeline> DefaultPipelineBuilder::build(
    const std::string& pipelineName)
{
    NX_OUTPUT << __func__ << " Building pipeline " << pipelineName;
    auto pipeline =
        std::make_unique<DefaultPipeline>(pipelineName);

    NX_OUTPUT << __func__ << " Creating elements and bins for " << pipelineName;
    auto appSource = createAppSource(pipelineName);
    auto preGieBin = buildPreGieBin(pipelineName);
    auto primaryGieBin = buildPrimaryGieBin(pipelineName, pipeline.get());
    auto trackerBin = buildTrackerBin(pipelineName, pipeline.get());
    auto secondaryGieBin = buildSecondaryGieBin(pipelineName, pipeline.get());
    auto fakeSink = createFakeSink(pipelineName);

    NX_OUTPUT << __func__ << " Adding elements and bins to pipeline " << pipelineName;
    pipeline->add(appSource.get());
    pipeline->add(preGieBin.get());
    pipeline->add(primaryGieBin.get());
    pipeline->add(trackerBin.get());
    pipeline->add(secondaryGieBin.get());
    pipeline->add(fakeSink.get());

    NX_OUTPUT << __func__ << " Linking elements and bins for pipeline " << pipelineName;
    preGieBin->linkAfter(appSource.get());
    // Primary GIE bin will be linked after dynamic pad is added to decode bin.
    trackerBin->linkAfter(primaryGieBin.get());
    secondaryGieBin->linkAfter(trackerBin.get());
    fakeSink->linkAfter(secondaryGieBin.get());

    NX_OUTPUT
        << __func__
        << " Connecting appsource 'need-data' signal for pipeline " << pipelineName;

    g_signal_connect(
        appSource->nativeElement(),
        "need-data",
        G_CALLBACK(appSourceNeedData),
        pipeline.get());

    NX_OUTPUT
        << __func__
        << " Connecting decoder bin 'pad-added' signal for pipeline " << pipelineName;

    auto pad = gst_element_get_static_pad(
        primaryGieBin->nativeElement(),
        "sink");

    g_signal_connect(
        preGieBin->nativeElement(),
        "pad-added",
        G_CALLBACK(decodeBinPadAdded),
        pad);

    g_object_unref(G_OBJECT(pad));

    NX_OUTPUT << __func__ << " Creating main loop and bus for pipeline " << pipelineName;
    auto mainLoop = LoopPtr(
        g_main_loop_new(NULL, false),
        [](GMainLoop* loop) { g_main_loop_unref(loop); });

    auto bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline->nativeElement()));
    gst_bus_add_watch(bus, busCallback, pipeline.get());
    gst_object_unref(bus);

    pipeline->setMainLoop(std::move(mainLoop));

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(
        GST_BIN(pipeline->nativeElement()),
        GST_DEBUG_GRAPH_SHOW_ALL,
        "nx-pipeline-null");

    auto trackingMapper = pipeline->trackingMapper();
    trackingMapper->setLabelMapping(makeLabelMapping());

    return pipeline;
}

std::unique_ptr<nx::gstreamer::Element> DefaultPipelineBuilder::buildPreGieBin(
    const nx::gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating decode bin for " << pipelineName;
    auto decodeBin = std::make_unique<nx::gstreamer::Element>(
        kGstElementDecodeBin,
        makeElementName(pipelineName, kGstElementDecodeBin, "mainDecodeBin"));

    return decodeBin;
}

std::unique_ptr<nx::gstreamer::Bin> DefaultPipelineBuilder::buildPrimaryGieBin(
    const nx::gstreamer::ElementName& pipelineName,
    DefaultPipeline* pipeline)
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

std::unique_ptr<nx::gstreamer::Bin> DefaultPipelineBuilder::buildTrackerBin(
    const nx::gstreamer::ElementName& pipelineName,
    DefaultPipeline* pipeline)
{
    NX_OUTPUT << __func__ << " Creating tracker bin for " << pipelineName;
    auto tracker = std::make_unique<nx::gstreamer::Element>(
        kNvidiaElementTracker,
        makeElementName(pipelineName, kNvidiaElementTracker, "tracker"));

    auto bin =  std::make_unique<nx::gstreamer::Bin>(
        makeElementName(pipelineName, "bin", "trackerBin"));

    bin->add(tracker.get());
    bin->createGhostPad(tracker.get(), kSinkPadName);
    bin->createGhostPad(tracker.get(), kSourcePadName);

    return bin;
}

std::unique_ptr<nx::gstreamer::Bin> DefaultPipelineBuilder::buildSecondaryGieBin(
    const nx::gstreamer::ElementName& pipelineName,
    DefaultPipeline* pipeline)
{
    NX_OUTPUT << __func__ << " Creating secondary GIE bin for " << pipelineName;
    auto bin = std::make_unique<nx::gstreamer::Bin>(
        makeElementName(pipelineName, "bin", "sgieBin"));

    auto tee = std::make_unique<nx::gstreamer::Element>(
        kGstElementTee,
        makeElementName(pipelineName, kGstElementTee, "sgieTee"));

    auto outputQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "sgieOutputQueue"));

    // Config dependent number of classifiers.
    auto secondaryGieQueue = std::make_unique<nx::gstreamer::Element>(
        kGstElementQueue,
        makeElementName(pipelineName, kGstElementQueue, "sgieQueue0"));

    auto secondaryGie = std::make_unique<nx::gstreamer::Element>(
        kNvidiaElementGie,
        makeElementName(pipelineName, kNvidiaElementGie, "sgieInferenceEngine0"));

    auto secondaryGieFakeSink = std::make_unique<nx::gstreamer::Element>(
        kGstElementFakeSink,
        makeElementName(pipelineName, kGstElementFakeSink, "sgieFakeSink"));

    bin->add(secondaryGieQueue.get());
    bin->add(secondaryGie.get());
    bin->add(secondaryGieFakeSink.get());

// ---------------------------------------------

    bin->add(tee.get());
    bin->add(outputQueue.get());

    secondaryGieQueue->linkToRequestPad(
        tee.get(),
        "src_%u",
        kSinkPadName);

    outputQueue->linkToRequestPad(
        tee.get(),
        "src_%u",
        kSinkPadName);

    secondaryGie->linkAfter(secondaryGieQueue.get());
    secondaryGieFakeSink->linkAfter(secondaryGie.get());

    bin->createGhostPad(tee.get(), kSinkPadName);
    bin->createGhostPad(outputQueue.get(), kSourcePadName);

    secondaryGie->setProperties(
        {
            nx::gstreamer::makeProperty("net-scale-factor", ini().sgie0_netScaleFactor),
            nx::gstreamer::makeProperty("model-path", ini().sgie0_modelFile),
            nx::gstreamer::makeProperty("protofile-path", ini().sgie0_protoFile),
            nx::gstreamer::makeProperty(
                "meanfile-path",
                (const gchar*)NULL/*ini().sgie0_meanFile*/),
            nx::gstreamer::makeProperty("labelfile-path", ini().sgie0_labelFile),
            nx::gstreamer::makeProperty("net-stride", (guint) ini().sgie0_netStride),
            nx::gstreamer::makeProperty("batch-size", (guint) ini().sgie0_batchSize),
            nx::gstreamer::makeProperty("gie-mode", guint{2}),
            nx::gstreamer::makeProperty("interval", (guint) ini().sgie0_inferenceInterval),
            nx::gstreamer::makeProperty("detected-min-w-h", ini().sgie0_minDetectedWH),
            nx::gstreamer::makeProperty("detected-max-w-h", ini().sgie0_maxDetectedWH),
            nx::gstreamer::makeProperty("model-cache", ini().sgie0_cacheFile),
            nx::gstreamer::makeProperty("model-color-format", ini().sgie0_colorFormat),
            nx::gstreamer::makeProperty("class-thresh-params", ini().sgie0_classThresholds),
            nx::gstreamer::makeProperty("detect-clr", guint{0}),
            nx::gstreamer::makeProperty("force-fp32", ini().sgie0_forceFp32),
            nx::gstreamer::makeProperty("enable-dbscan", ini().sgie0_enableDbScan),
            nx::gstreamer::makeProperty("crypto-flags", gboolean{0}),
            nx::gstreamer::makeProperty(
                "sec-class-threshold",
                ini().sgie0_secondaryGieClassifierThreshold),
            nx::gstreamer::makeProperty("gie-unique-id", ini().sgie0_gieUniqueId),
            nx::gstreamer::makeProperty("infer-on-gie-id", ini().sgie0_makeInferenceOnGieWithId),
            nx::gstreamer::makeProperty(
                "infer-on-class-ids",
                ini().sgie0_makeInferenceOnClassIds),
            nx::gstreamer::makeProperty("is-classifier", ini().sgie0_isClassifier),
            nx::gstreamer::makeProperty("offsets", ini().sgie0_colorOffsets),
            nx::gstreamer::makeProperty(
                "output-bbox-layer-name",
                ini().sgie0_outputBboxLayerName),
            nx::gstreamer::makeProperty(
                "output-coverage-layer-names",
                ini().sgie0_outputCoverageLayerNames),

            nx::gstreamer::makeProperty("parse-func", guint{0}),
            nx::gstreamer::makeProperty("parse-bbox-func-name", (const gchar*)NULL),
            nx::gstreamer::makeProperty("parse-bbox-lib-name", (const gchar*)NULL)
        });

    auto probePad = gst_element_get_static_pad(
        bin->nativeElement(),
        kSourcePadName);

    gst_pad_add_probe(
        probePad,
        GST_PAD_PROBE_TYPE_BUFFER,
        waitForSecondaryGieDoneBufProbe,
        pipeline,
        NULL);

    gst_object_unref(GST_OBJECT(probePad));

    return bin;
}

std::unique_ptr<nx::gstreamer::Element> DefaultPipelineBuilder::createAppSource(
    const nx::gstreamer::ElementName& pipelineName)
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
            NULL),
        "is-live",
        TRUE,
        NULL);

    return appSource;
}

std::unique_ptr<gstreamer::Element> DefaultPipelineBuilder::createFakeSink(
    const nx::gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Creating fake sink for pipeline " << pipelineName;
    auto fakeSink = std::make_unique<nx::gstreamer::Element>(
        kGstElementFakeSink,
        makeElementName(pipelineName, kGstElementFakeSink, "fakeSink"));

    return fakeSink;
}

std::map<LabelMappingId, LabelMapping> DefaultPipelineBuilder::makeLabelMapping()
{
    NX_OUTPUT << __func__ << " Making label mapping from label files files";
    std::map<LabelMappingId, LabelMapping> result;
    auto rawLabels = parseLabelFile(ini().sgie0_labelFile);

    LabelMapping labelMapping;
    for (auto i = 0; i < rawLabels.size(); ++i)
    {
        LabelTypeMapping labelTypeMapping;
        labelTypeMapping.labelTypeString = "Vehicle type"; //< TODO: #dmishin remove hardcode!
        for (auto j = 0; j < rawLabels[i].size(); ++j)
            labelTypeMapping.labelValueMapping.emplace(j, rawLabels[i][j]);

        labelMapping.emplace(i, labelTypeMapping);
    }

    result.emplace(ini().sgie0_gieUniqueId, labelMapping);
    return result;
}

RawLabels DefaultPipelineBuilder::parseLabelFile(const std::string& path) const
{
    RawLabels result;
    std::ifstream labelFile(path, std::ios::in);

    std::string line;
    while (std::getline(labelFile, line))
    {
        if (trim(&line)->empty())
            continue;

        result.push_back(std::vector<std::string>());
        auto labelSplit = split(line, ";");

        for (auto& label: labelSplit)
            result.back().push_back(*trim(&label));
    }

    return result;
}

std::string DefaultPipelineBuilder::makeElementName(
    const nx::gstreamer::ElementName& pipelineName,
    const nx::gstreamer::FactoryName& factoryName,
    const nx::gstreamer::ElementName& elementName)
{
    return pipelineName + "_" + factoryName + "_" + elementName;
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
