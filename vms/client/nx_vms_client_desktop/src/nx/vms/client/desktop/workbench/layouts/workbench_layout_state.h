// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/instrument.h>
#include <nx/utils/datetime.h>

namespace nx::vms::client::desktop {

struct StreamSynchronizationState
{
    static StreamSynchronizationState live() { return {true, DATETIME_NOW, 1.0}; }
    static StreamSynchronizationState disabled() { return {false, DATETIME_INVALID, 0.0}; }
    static StreamSynchronizationState playFromStart() { return {true, 0, 1.0}; }

    bool isSyncOn = false;
    qint64 timeUs = DATETIME_INVALID;
    qreal speed = 0.0;
};
#define StreamSynchronizationState_Fields (isSyncOn)(timeUs)(speed)

NX_REFLECTION_INSTRUMENT(StreamSynchronizationState, StreamSynchronizationState_Fields)

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::StreamSynchronizationState)
