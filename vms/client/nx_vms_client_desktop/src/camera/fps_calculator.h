// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int *maxFps);

/** Returns max fps. */
int calculateMaxFps(const QnVirtualCameraResourceList& cameras);

/** Returns max fps. */
int calculateMaxFps(const QnVirtualCameraResourcePtr& camera);

} // namespace Qn
