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
