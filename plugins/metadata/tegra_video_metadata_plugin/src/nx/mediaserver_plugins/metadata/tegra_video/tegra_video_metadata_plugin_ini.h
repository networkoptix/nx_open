#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

static const char* const kZoneCorrection = "Correction in the specified frame zone";

/**
 * Used by libtegra_video_metadata_plugin.so.
 */
struct IniConfig: public nx::kit::IniConfig
{
    IniConfig(): nx::kit::IniConfig("tegra_video_metadata_plugin.ini")
    {
        reload();
    }

    NX_INI_STRING("none", postprocType, "Postprocessing type of net rects: none|ped|car.");

    NX_INI_FLAG(1, enableOutput, ""); //< TODO: #mike: Change to 0.
    NX_INI_FLAG(0, enableTime, "");

    NX_INI_STRING("/opt/networkoptix/mediaserver/nvidia_models/carnet.prototxt", deployFile, "");
    NX_INI_STRING("/opt/networkoptix/mediaserver/nvidia_models/carnet.caffemodel", modelFile, "");
    NX_INI_STRING("/opt/networkoptix/mediaserver/var/cuda_engine.cache", cacheFile, "");

    NX_INI_INT(0, netWidth, "Net input width (pixels). If 0, try to parse from deployFile.");
    NX_INI_INT(0, netHeight, "Net input height (pixels). If 0, try to parse from deployFile.");

    //---------------------------------------------------------------------------------------------

    NX_INI_INT(0, postprocMinObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(0, postprocMinObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(35, postprocMaxObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(25, postprocMaxObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(30, postprocSimilarityThreshold,
        "Minimal distance between detected objects to consider them as the same object.");

    NX_INI_INT(3, postprocObjectLifetime, "Number of frames to keep rectangle for lost object.");

    NX_INI_FLAG(1, postprocApplySpeedToCachedRectangles, "");
    NX_INI_FLAG(1, postprocApplySpeedForDistanceCalculation, "");

    NX_INI_INT(-3, postprocXfirstZoneCorrection, kZoneCorrection);
    NX_INI_INT(20, postprocYfirstZoneCorrection, kZoneCorrection);
    NX_INI_INT(0, postprocXsecondZoneCorrection, kZoneCorrection);
    NX_INI_INT(0, postprocYsecondZoneCorrection, kZoneCorrection);
    NX_INI_INT(3, postprocXthirdZoneCorrection, kZoneCorrection);
    NX_INI_INT(20, postprocYthirdZoneCorrection, kZoneCorrection);

    NX_INI_INT(40, postprocFirstZoneBound, "Should be divided by 100.");
    NX_INI_INT(70, postprocSecondZoneBound, "Should be divided by 100.");

    NX_INI_INT(0, postprocBottomBorderLifetime, "");
    NX_INI_INT(90, postprocBottomBorderBound, "Should be divided by 100.");

    NX_INI_INT(10, postprocDefaultYSpeed, "Should be divided by 1000.");

    NX_INI_INT(5, postprocMaxBackVectorLength, "Should be divided by 1000.");

    NX_INI_STRING("continuousSequence", postprocSmoothingFilterType,
        "Type of smoothing filter: slidingWindow|continuousSequence.");

    NX_INI_INT(1, postprocSlidingWindowSize,
        "Frame sequence length to decide if there are humans on the scene (sliding window).");

    NX_INI_INT(1, postprocActivationSequenceLength,
        "Frame sequence length to decide if there are humans on the scene.");

    NX_INI_INT(1, postprocDeactivationSequenceLength,
        "Frame sequence length to decide if there are no humans on the scene.");

    NX_INI_INT(50, postProcNoCorrectionAlpha, "aX + b alpha coefficient.");
    NX_INI_INT(50, postProcNoCorrectionBeta, "aX + b beta coefficient.");
};

inline IniConfig& ini()
{
    static IniConfig ini;
    return ini;
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
