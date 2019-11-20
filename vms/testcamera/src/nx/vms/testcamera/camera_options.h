#pragma once

#include <cstdint>

namespace nx::vms::testcamera {

struct CameraOptions
{
    bool includePts = false;
    bool unloopPts = false;
    int64_t shiftPtsPrimaryPeriodUs = -1;
    int64_t shiftPtsSecondaryPeriodUs = -1;
    int offlineFreq = 0;
};

} // namespace nx::vms::testcamera
