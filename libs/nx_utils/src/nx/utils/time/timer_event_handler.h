// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

namespace nx::utils {

using TimerId = quint64;

/**
 * Abstract interface for receiving timer events.
 */
class NX_UTILS_API TimerEventHandler
{
public:
    virtual ~TimerEventHandler() = default;

    /**
     * Called on timer event.
     * @param timerId
     */
    virtual void onTimer(const TimerId& timerId) = 0;
};

} // namespace nx::utils
