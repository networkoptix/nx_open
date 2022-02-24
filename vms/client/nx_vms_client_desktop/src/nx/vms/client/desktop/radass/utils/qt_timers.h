// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_timers.h"

namespace nx::vms::client::desktop {

/**
 * Implements factory to create Qt-based timers.
 */
class QtTimerFactory: public AbstractTimerFactory
{
public:
    virtual AbstractTimer* createTimer(QObject* parent = nullptr) override;
    virtual AbstractElapsedTimer* createElapsedTimer() override;
};

} // namespace nx::vms::client::desktop
