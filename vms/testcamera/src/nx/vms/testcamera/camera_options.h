#pragma once

#include <optional>
#include <chrono>

namespace nx::vms::testcamera {

/**
 * Options for a particular camera, filled from CLI arguments and passed via CameraPool to Camera.
 */
struct CameraOptions
{
    using OptionalUs = std::optional<std::chrono::microseconds>;

    bool includePts = false;
    OptionalUs shiftPts;
    bool unloopPts = false;
    OptionalUs shiftPtsFromNow;
    OptionalUs shiftPtsPrimaryPeriod;
    OptionalUs shiftPtsSecondaryPeriod;
    int offlineFreq = 0;
};

} // namespace nx::vms::testcamera
