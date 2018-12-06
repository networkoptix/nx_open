#include "ptz_limits.h"

#include <utils/math/math.h>

#include <nx/utils/log/assert.h>

using namespace nx::core::ptz;

namespace {

double range(double min, double max)
{
    if (!qIsNaN(min) && !qIsNaN(max))
        return max - min;

    return qQNaN<double>();
}

} // namespace

double QnPtzLimits::maxComponentValue(nx::core::ptz::Component component) const
{
    switch(component)
    {
        case Component::pan: return maxPan;
        case Component::tilt: return maxTilt;
        case Component::rotation:return maxRotation;
        case Component::zoom: return maxFov;
        case Component::focus: return maxFocus;
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}

double QnPtzLimits::minComponentValue(nx::core::ptz::Component component) const
{
    switch (component)
    {
        case Component::pan: return minPan;
        case Component::tilt: return minTilt;
        case Component::rotation:return minRotation;
        case Component::zoom: return minFov;
        case Component::focus: return minFocus;
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}

double QnPtzLimits::maxComponentSpeed(nx::core::ptz::Component component) const
{
    switch (component)
    {
        case Component::pan: return maxPanSpeed;
        case Component::tilt: return maxTiltSpeed;
        case Component::rotation:return maxRotationSpeed;
        case Component::zoom: return maxZoomSpeed;
        case Component::focus: return maxFocusSpeed;
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}

double QnPtzLimits::minComponentSpeed(nx::core::ptz::Component component) const
{
    switch (component)
    {
        case Component::pan: return minPanSpeed;
        case Component::tilt: return minTiltSpeed;
        case Component::rotation:return minRotationSpeed;
        case Component::zoom: return minZoomSpeed;
        case Component::focus: return minFocusSpeed;
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}

double QnPtzLimits::componentRange(nx::core::ptz::Component component) const
{
    switch (component)
    {
        case Component::pan: return range(minPan, maxPan);
        case Component::tilt: return range(minTilt, maxTilt);
        case Component::rotation: return range(minRotation, maxRotation);
        case Component::zoom: return range(minFov, maxFov);
        case Component::focus: return range(minFocus, maxFocus);
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}

double QnPtzLimits::componentSpeedRange(nx::core::ptz::Component component) const
{
    switch (component)
    {
        case Component::pan: return range(minPanSpeed, maxPanSpeed);
        case Component::tilt: return range(minTiltSpeed, maxTiltSpeed);
        case Component::rotation: return range(minRotationSpeed, maxRotationSpeed);
        case Component::zoom: return range(minZoomSpeed, maxZoomSpeed);
        case Component::focus: return range(minFocusSpeed, maxFocusSpeed);
        default:
            NX_ASSERT(false, "Wrong component type.");
            return qQNaN<double>();
    }
}
