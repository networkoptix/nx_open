#include "secondary_gie_common.h"

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

std::vector<gstreamer::Property> giePropertiesByIndex(int index)
{
    switch (index)
    {
        case kFirstClassifier:
        {
            return {
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
                nx::gstreamer::makeProperty(
                    "infer-on-gie-id",
                    ini().sgie0_makeInferenceOnGieWithId),
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
            };
        }
        case kSecondClassifier:
        {
            return {
                nx::gstreamer::makeProperty("net-scale-factor", ini().sgie1_netScaleFactor),
                nx::gstreamer::makeProperty("model-path", ini().sgie1_modelFile),
                nx::gstreamer::makeProperty("protofile-path", ini().sgie1_protoFile),
                nx::gstreamer::makeProperty(
                    "meanfile-path",
                    (const gchar*)NULL/*ini().sgie1_meanFile*/),
                nx::gstreamer::makeProperty("labelfile-path", ini().sgie1_labelFile),
                nx::gstreamer::makeProperty("net-stride", (guint) ini().sgie1_netStride),
                nx::gstreamer::makeProperty("batch-size", (guint) ini().sgie1_batchSize),
                nx::gstreamer::makeProperty("gie-mode", guint{2}),
                nx::gstreamer::makeProperty("interval", (guint) ini().sgie1_inferenceInterval),
                nx::gstreamer::makeProperty("detected-min-w-h", ini().sgie1_minDetectedWH),
                nx::gstreamer::makeProperty("detected-max-w-h", ini().sgie1_maxDetectedWH),
                nx::gstreamer::makeProperty("model-cache", ini().sgie1_cacheFile),
                nx::gstreamer::makeProperty("model-color-format", ini().sgie1_colorFormat),
                nx::gstreamer::makeProperty("class-thresh-params", ini().sgie1_classThresholds),
                nx::gstreamer::makeProperty("detect-clr", guint{0}),
                nx::gstreamer::makeProperty("force-fp32", ini().sgie1_forceFp32),
                nx::gstreamer::makeProperty("enable-dbscan", ini().sgie1_enableDbScan),
                nx::gstreamer::makeProperty("crypto-flags", gboolean{0}),
                nx::gstreamer::makeProperty(
                    "sec-class-threshold",
                    ini().sgie1_secondaryGieClassifierThreshold),
                nx::gstreamer::makeProperty("gie-unique-id", ini().sgie1_gieUniqueId),
                nx::gstreamer::makeProperty(
                    "infer-on-gie-id",
                    ini().sgie1_makeInferenceOnGieWithId),
                nx::gstreamer::makeProperty(
                    "infer-on-class-ids",
                    ini().sgie1_makeInferenceOnClassIds),
                nx::gstreamer::makeProperty("is-classifier", ini().sgie1_isClassifier),
                nx::gstreamer::makeProperty("offsets", ini().sgie1_colorOffsets),
                nx::gstreamer::makeProperty(
                    "output-bbox-layer-name",
                    ini().sgie1_outputBboxLayerName),
                nx::gstreamer::makeProperty(
                    "output-coverage-layer-names",
                    ini().sgie1_outputCoverageLayerNames),

                nx::gstreamer::makeProperty("parse-func", guint{0}),
                nx::gstreamer::makeProperty("parse-bbox-func-name", (const gchar*)NULL),
                nx::gstreamer::makeProperty("parse-bbox-lib-name", (const gchar*)NULL)
            };
        }
        case kThirdClassifier:
        {
            return {
                nx::gstreamer::makeProperty("net-scale-factor", ini().sgie2_netScaleFactor),
                nx::gstreamer::makeProperty("model-path", ini().sgie2_modelFile),
                nx::gstreamer::makeProperty("protofile-path", ini().sgie2_protoFile),
                nx::gstreamer::makeProperty(
                    "meanfile-path",
                    (const gchar*)NULL/*ini().sgie2_meanFile*/),
                nx::gstreamer::makeProperty("labelfile-path", ini().sgie2_labelFile),
                nx::gstreamer::makeProperty("net-stride", (guint) ini().sgie2_netStride),
                nx::gstreamer::makeProperty("batch-size", (guint) ini().sgie2_batchSize),
                nx::gstreamer::makeProperty("gie-mode", guint{2}),
                nx::gstreamer::makeProperty("interval", (guint) ini().sgie2_inferenceInterval),
                nx::gstreamer::makeProperty("detected-min-w-h", ini().sgie2_minDetectedWH),
                nx::gstreamer::makeProperty("detected-max-w-h", ini().sgie2_maxDetectedWH),
                nx::gstreamer::makeProperty("model-cache", ini().sgie2_cacheFile),
                nx::gstreamer::makeProperty("model-color-format", ini().sgie2_colorFormat),
                nx::gstreamer::makeProperty("class-thresh-params", ini().sgie2_classThresholds),
                nx::gstreamer::makeProperty("detect-clr", guint{0}),
                nx::gstreamer::makeProperty("force-fp32", ini().sgie2_forceFp32),
                nx::gstreamer::makeProperty("enable-dbscan", ini().sgie2_enableDbScan),
                nx::gstreamer::makeProperty("crypto-flags", gboolean{0}),
                nx::gstreamer::makeProperty(
                    "sec-class-threshold",
                    ini().sgie2_secondaryGieClassifierThreshold),
                nx::gstreamer::makeProperty("gie-unique-id", ini().sgie2_gieUniqueId),
                nx::gstreamer::makeProperty(
                    "infer-on-gie-id",
                    ini().sgie2_makeInferenceOnGieWithId),
                nx::gstreamer::makeProperty(
                    "infer-on-class-ids",
                    ini().sgie2_makeInferenceOnClassIds),
                nx::gstreamer::makeProperty("is-classifier", ini().sgie2_isClassifier),
                nx::gstreamer::makeProperty("offsets", ini().sgie2_colorOffsets),
                nx::gstreamer::makeProperty(
                    "output-bbox-layer-name",
                    ini().sgie2_outputBboxLayerName),
                nx::gstreamer::makeProperty(
                    "output-coverage-layer-names",
                    ini().sgie2_outputCoverageLayerNames),

                nx::gstreamer::makeProperty("parse-func", guint{0}),
                nx::gstreamer::makeProperty("parse-bbox-func-name", (const gchar*)NULL),
                nx::gstreamer::makeProperty("parse-bbox-lib-name", (const gchar*)NULL)
            };
        }
        default:
        {
            return {};
        }
    }
}

bool isSecondaryGieEnabled(int index)
{
    switch (index)
    {
        case kFirstClassifier:
            return ini().sgie0_enabled;
        case kSecondClassifier:
            return ini().sgie1_enabled;
        case kThirdClassifier:
            return ini().sgie2_enabled;
        default:
            return false;
    }
}

std::string labelFileByIndex(int index)
{
    switch (index)
    {
        case kFirstClassifier:
            return ini().sgie0_labelFile;
        case kSecondClassifier:
            return ini().sgie1_labelFile;
        case kThirdClassifier:
            return ini().sgie2_labelFile;
        default:
            return std::string();
    }
}

int uniqueIdByIndex(int index)
{
    switch (index)
    {
        case kFirstClassifier:
            return ini().sgie0_gieUniqueId;
        case kSecondClassifier:
            return ini().sgie1_gieUniqueId;
        case kThirdClassifier:
            return ini().sgie2_gieUniqueId;
        default:
            return -1;
    }
}

std::string labelTypeByIndex(int index)
{
    switch (index)
    {
        case kFirstClassifier:
            return ini().sgie0_labelType;
        case kSecondClassifier:
            return ini().sgie1_labelType;
        case kThirdClassifier:
            return ini().sgie2_labelType;
        default:
            return "Unknown label";
    }
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
