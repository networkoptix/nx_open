#pragma once

#include <cstdint>

namespace nx::vms::testcamera {

/**
 * Options for a particular camera, filled from CLI arguments and passed via CameraPool to Camera.
 */
struct CameraOptions
{
    bool includePts = false;
    bool unloopPts = false;
    int64_t shiftPtsPrimaryPeriodUs = -1;
    int64_t shiftPtsSecondaryPeriodUs = -1;
    int offlineFreq = 0;
};

} // namespace nx::vms::testcamera
