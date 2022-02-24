// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int *maxFps,
    int *maxDualStreamFps,
    bool motionDetectionAllowed = true);

/** Returns max fps as first pair value and max dual streaming fps as second. */
QPair<int, int> calculateMaxFps(const QnVirtualCameraResourceList& cameras,
    bool motionDetectionAllowed = true);

/** Returns max fps as first pair value and max dual streaming fps as second. */
QPair<int, int> calculateMaxFps(const QnVirtualCameraResourcePtr& camera,
    bool motionDetectionAllowed = true);

} // namespace Qn
