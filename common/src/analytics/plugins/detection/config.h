#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace analytics {

#define NX_ANALYTICS_PATH "/opt/networkoptix/mediaserver/bin/nvidia_models"
#define NX_VAR_PATH "/opt/networkoptix/mediaserver/var"
#define NX_ZONE_CORRECTION_COMMENT "Compensates constant metadata lag. Should be divided by 1000"

struct IniConfig: public nx::kit::IniConfig
{
    IniConfig(): nx::kit::IniConfig("analytics.ini") { reload(); }

    // Common parameters
    NX_INI_STRING("", pathToPlugin, "");
    NX_INI_FLAG(0, enableOutput, "Enable debug output.");
    NX_INI_STRING(
        "",
        dumpCompressedFrames,
        "If not empty, save compressed frames as <this_param>.#");
    NX_INI_STRING("", substituteFramesFilePrefix, "");
    NX_INI_FLAG(0, singleInstance, "");
    NX_INI_FLAG(0, useTestImplementation, "");
    NX_INI_INT(0, timestampShift, "Shifts timestamp of recognized objects.");
    NX_INI_INT(1920, frameWidth, "");
    NX_INI_INT(1080, frameHeight, "");

    NX_INI_STRING(NX_ANALYTICS_PATH "/pednet.prototxt", deployFile, "");
    NX_INI_STRING(NX_ANALYTICS_PATH "/pednet.caffemodel", modelFile, "");
    NX_INI_STRING(NX_VAR_PATH "/var/cuda_engine.cache", cacheFile, "");

    NX_INI_FLAG(0, enableDetectionPlugin, "");
    NX_INI_FLAG(1, enableMotionDetection, "");

    // Rectangles filtering parameters
    NX_INI_INT(
        0,
        minObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");
    NX_INI_INT(
        0,
        minObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100.");

    NX_INI_INT(
        35,
        maxObjectWidth,
        "Rectangle width in 0..1 coordinates. Should be divided by 100.");
    NX_INI_INT(
        25,
        maxObjectHeight,
        "Rectangle height in 0..1 coordinates. Should be divided by 100");

    // Object tracking parameters
    NX_INI_FLAG(
        0,
        enableNaiveObjectTracking,
        "Enable coordinate difference based object tracking.");

    NX_INI_INT(
        50,
        similarityThreshold,
        "Minimal distance between detected objects to consider them as the same object.");

    NX_INI_INT(1, objectLifetime, "Number of frames to keep rectangle for lost object.");

    NX_INI_FLAG(1, applySpeedToCachedRectangles, "");
    NX_INI_FLAG(1, applySpeedForDistanceCalculation, "");

    NX_INI_INT(20, xFirstZoneCorrection, NX_ZONE_CORRECTION_COMMENT);
    NX_INI_INT(-40, yFirstZoneCorrection, NX_ZONE_CORRECTION_COMMENT);
    NX_INI_INT(0, xSecondZoneCorrection, NX_ZONE_CORRECTION_COMMENT);
    NX_INI_INT(40, ySecondZoneCorrection, NX_ZONE_CORRECTION_COMMENT);
    NX_INI_INT(20, xThirdZoneCorrection, NX_ZONE_CORRECTION_COMMENT);
    NX_INI_INT(40, yThirdZoneCorrection, NX_ZONE_CORRECTION_COMMENT);

    NX_INI_INT(40, firstZoneBound, "Should be divided by 100");
    NX_INI_INT(70, secondZoneBound, "Should be divided by 100");

    NX_INI_INT(2, bottomBorderLifetime, "");
    NX_INI_INT(90, bottomBorderBound, "Should be divided by 100");

    NX_INI_INT(40, defaultYSpeed, "Should be divided by 1000");

    NX_INI_INT(5, maxBackVectorLength, "Should be divided by 1000");
    NX_INI_STRING("pednet", netType, "Network type. Available types are 'pednet' and 'carnet'");

    // Common inference parameters
    NX_INI_STRING("Person detected", detectionStartCaption, "");
    NX_INI_STRING("Person left", detectionEndCaption, "");
    NX_INI_STRING("There are men nearby", detectionStartDescription, "");
    NX_INI_STRING("There are no men nearby", detectionEndDescription, "");

    NX_INI_FLAG(0, useKeyFramesOnly, "Decode and perform inference for key frames only.");

    NX_INI_STRING(
        "primary",
        inferenceStream,
        "Stream to use for inference. "
        "Available values are 'primary' and 'secondary'");

    NX_INI_STRING(
        "continuousSequence",
        smoothingFilterType,
        "Type of smoothing filter. "
        "Available values are 'slidingWindow' and 'continuousSequence'");

    NX_INI_INT(
        1,
        slidingWindowSize,
        "Length of sequence of frames to decide "
        "if there are people on the scene (slidingWindow filter)");

    NX_INI_INT(
        1,
        activationSequenceLength,
        "Length of sequence of frames to state that there are men on the scene");

    NX_INI_INT(
        1,
        deactivationSequenceLength,
        "Length of sequence of frames to state that there are no men on the scene");
};

inline IniConfig& ini()
{
    static IniConfig ini;
    return ini;
}

#undef NX_VAR_PATH
#undef NX_ANALYTICS_PATH

} // namespace analytics
} // namespace nx
