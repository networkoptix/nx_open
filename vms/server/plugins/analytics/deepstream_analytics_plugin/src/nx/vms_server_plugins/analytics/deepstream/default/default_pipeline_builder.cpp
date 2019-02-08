#include "default_pipeline_builder.h"

#include <iostream>
#include <fstream>

#include "secondary_gie_builder.h"
#include "secondary_gie_common.h"
#include "default_pipeline_callbacks.h"

#include <nx/vms_server_plugins/analytics/deepstream/utils.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>

#define NX_PRINT_PREFIX "deepstream::DefaultPipelineBuilder::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

DefaultPipelineBuilder::DefaultPipelineBuilder(deepstream::Engine* engine):
    base_type(engine)
{
    ini().reload();
}

std::unique_ptr<nx::gstreamer::Pipeline> DefaultPipelineBuilder::build(
    const nx::gstreamer::ElementName& pipelineName)
{
    NX_OUTPUT << __func__ << " Building pipeline " << pipelineName;
    auto pipeline =
        std::make_unique<DefaultPipeline>(pipelineName, m_engine);

    NX_OUTPUT << __func__ << " Creating elements and bins for " << pipelineName;
    auto appSource = createAppSource(pipeline.get(), pipelineName);
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

    connectOnPadAdded(
        preGieBin.get(),
        primaryGieBin.get(),
        pipelineName);

    createMainLoop(pipeline.get(), pipelineName);

    auto trackingMapper = pipeline->trackingMapper();
    trackingMapper->setLabelMapping(makeLabelMapping());

    return pipeline;
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
    return SecondaryGieBuilder::buildSecondaryGie(
        pipelineName,
        pipeline);
}

std::map<LabelMappingId, LabelMapping> DefaultPipelineBuilder::makeLabelMapping()
{
    std::map<LabelMappingId, LabelMapping> result;

    for (auto i = 0; i < kMaxClassifiers; ++i)
    {
        if (!isSecondaryGieEnabled(i))
            continue;

        const auto labelFile = labelFileByIndex(i);
        if (labelFile.empty())
            continue;

        auto rawLabels = parseLabelFile(labelFile);
        NX_OUTPUT << __func__ << " Making label mapping from label files files";
        LabelMapping labelMapping;
        for (auto j = 0; j < rawLabels.size(); ++j)
        {
            LabelTypeMapping labelTypeMapping;
            labelTypeMapping.labelTypeString = labelTypeByIndex(i);
            for (auto k = 0; k < rawLabels[j].size(); ++k)
                labelTypeMapping.labelValueMapping.emplace(k, rawLabels[j][k]);

            labelMapping.emplace(j, labelTypeMapping);
        }

        result.emplace(uniqueIdByIndex(i), labelMapping);
    }

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

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
