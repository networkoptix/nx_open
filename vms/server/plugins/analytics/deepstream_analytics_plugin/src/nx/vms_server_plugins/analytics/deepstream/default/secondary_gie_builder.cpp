#include "secondary_gie_builder.h"

#include "secondary_gie_common.h"
#include "default_pipeline_callbacks.h"

#include <nx/vms_server_plugins/analytics/deepstream/utils.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::DefaultPipelineBuilder::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

std::unique_ptr<gstreamer::Bin> SecondaryGieBuilder::buildSecondaryGie(
    const gstreamer::ElementName& pipelineName,
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

    bin->add(tee.get());
    bin->add(outputQueue.get());

    outputQueue->linkToRequestPad(
        tee.get(),
        "src_%u",
        kSinkPadName);

    for (auto i = 0; i < kMaxClassifiers; ++i)
    {
        if (!isSecondaryGieEnabled(i))
            continue;

        auto secondaryGieQueue = std::make_unique<nx::gstreamer::Element>(
            kGstElementQueue,
            makeElementName(pipelineName, kGstElementQueue, "sgieQueue_" + std::to_string(i)));

        auto secondaryGie = std::make_unique<nx::gstreamer::Element>(
            kNvidiaElementGie,
            makeElementName(
                pipelineName,
                kNvidiaElementGie,
                "sgieInferenceEngine_" + std::to_string(i)));

        auto secondaryGieFakeSink = std::make_unique<nx::gstreamer::Element>(
            kGstElementFakeSink,
            makeElementName(
                pipelineName,
                kGstElementFakeSink,
                "sgieFakeSink_" + std::to_string(i)));

        bin->add(secondaryGieQueue.get());
        bin->add(secondaryGie.get());
        bin->add(secondaryGieFakeSink.get());

        secondaryGieQueue->linkToRequestPad(
            tee.get(),
            "src_%u",
            kSinkPadName);

        secondaryGie->linkAfter(secondaryGieQueue.get());
        secondaryGieFakeSink->linkAfter(secondaryGie.get());
        secondaryGie->setProperties(giePropertiesByIndex(i));
    }

    bin->createGhostPad(tee.get(), kSinkPadName);
    bin->createGhostPad(outputQueue.get(), kSourcePadName);

    bin->pad(kSourcePadName)
        ->addProbe(waitForSecondaryGieDoneBufProbe, GST_PAD_PROBE_TYPE_BUFFER, pipeline);

    return bin;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
