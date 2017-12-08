#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

/**
 * Used by libtegra_video_metadata_plugin.so.
 */

static const char* kZoneCorrection = "Correction in the specified frame zone";

struct TegraVideoMetadataPluginIniConfig: public nx::kit::IniConfig
{
    TegraVideoMetadataPluginIniConfig(): nx::kit::IniConfig("tegra_video_metadata_plugin.ini")
    {
        reload();
    }

    NX_INI_STRING("none", postprocType, "Postprocessing type of net rects: none|ped|car.");

    NX_INI_FLAG(1, enableOutput, ""); //< TODO: #mike: Change to 0.
    NX_INI_FLAG(0, enableTime, "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/carnet.prototxt",
        deployFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/nvidia_models/carnet.caffemodel",
        modelFile,
        "");

    NX_INI_STRING(
        "/opt/networkoptix/mediaserver/var/cuda_engine.cache",
        cacheFile,
        "");

    NX_INI_INT(0, netWidth, "Net input width (pixels). If 0, try to parse from deployFile.");
    NX_INI_INT(0, netHeight, "Net input height (pixels). If 0, try to parse from deployFile.");

    //---------------------------------------------------------------------------------------------

    NX_INI_INT(
        0,
        postProcMinObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(
        0,
        postProcMinObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(
        35,
        postProcMaxObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");
    NX_INI_INT(
        25,
        postProcMaxObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100");

    NX_INI_INT(
        50,
        postProcSimilarityThreshold,
        "Minimal distance between detected objects to consider them as the same object.");

    NX_INI_INT(1, postProcObjectLifetime, "Number of frames to keep rectangle for lost object.");

    NX_INI_FLAG(1, postProcApplySpeedToCachedRectangles, "");
    NX_INI_FLAG(1, postProcApplySpeedForDistanceCalculation, "");

    NX_INI_INT(20, postProcXfirstZoneCorrection, kZoneCorrection);
    NX_INI_INT(-40, postProcYfirstZoneCorrection, kZoneCorrection);
    NX_INI_INT(0, postProcXsecondZoneCorrection, kZoneCorrection);
    NX_INI_INT(40, postProcYsecondZoneCorrection, kZoneCorrection);
    NX_INI_INT(20, postProcXthirdZoneCorrection, kZoneCorrection);
    NX_INI_INT(40, postProcYthirdZoneCorrection, kZoneCorrection);

    NX_INI_INT(40, postProcFirstZoneBound, "Should be divided by 100");
    NX_INI_INT(70, postProcSecondZoneBound, "Should be divided by 100");

    NX_INI_INT(2, postProcBottomBorderLifetime, "");
    NX_INI_INT(90, postProcBottomBorderBound, "Should be divided by 100");

    NX_INI_INT(40, postProcDefaultYSpeed, "Should be divided by 1000");

    NX_INI_INT(5, postProcMaxBackVectorLength, "Should be divided by 1000");

    NX_INI_STRING(
        "continuousSequence",
        postProcSmoothingFilterType,
        "Type of smoothing filter. "
        "Available values are 'slidingWindow' and 'continuousSequence'");

    NX_INI_INT(
        1,
        postProcSlidingWindowSize,
        "Length of sequence of frames to decide "
        "if there are people on the scene (slidingWindow filter)");

    NX_INI_INT(
        1,
        postProcActivationSequenceLength,
        "Length of sequence of frames to state that there are men on the scene");

    NX_INI_INT(
        1,
        postProcDeactivationSequenceLength,
        "Length of sequence of frames to state that there are no men on the scene");
};

inline TegraVideoMetadataPluginIniConfig& ini()
{
    static TegraVideoMetadataPluginIniConfig ini;
    return ini;
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
