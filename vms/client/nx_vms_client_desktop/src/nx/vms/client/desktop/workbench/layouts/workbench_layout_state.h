// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/datetime.h>

namespace nx::vms::client::desktop {

struct StreamSynchronizationState
{
    static StreamSynchronizationState live()
    {
        return {.isSyncOn = true, .timeUs = DATETIME_NOW, .speed = 1.0};
    }
    static StreamSynchronizationState disabled()
    {
        return {};
    }
    static StreamSynchronizationState playFromStart()
    {
        return {.isSyncOn = true, .timeUs = 0, .speed = 1.0};
    }

    bool isSyncOn = false;
    qint64 timeUs = DATETIME_INVALID;
    qreal speed = 0.0;
};
#define StreamSynchronizationState_Fields (isSyncOn)(timeUs)(speed)

NX_REFLECTION_INSTRUMENT(StreamSynchronizationState, StreamSynchronizationState_Fields)

} // namespace nx::vms::client::desktop
