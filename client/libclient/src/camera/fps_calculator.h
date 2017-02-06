#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int *maxFps,
    int *maxDualStreamFps,
    Qn::MotionType motionTypeOverride = Qn::MT_Default);

/** Returns max fps as first pair value and max dual streaming fps as second. */
QPair<int, int> calculateMaxFps(const QnVirtualCameraResourceList& cameras,
    Qn::MotionType motionTypeOverride = Qn::MT_Default);

/** Returns max fps as first pair value and max dual streaming fps as second. */
QPair<int, int> calculateMaxFps(const QnVirtualCameraResourcePtr& camera,
    Qn::MotionType motionTypeOverride = Qn::MT_Default);

}