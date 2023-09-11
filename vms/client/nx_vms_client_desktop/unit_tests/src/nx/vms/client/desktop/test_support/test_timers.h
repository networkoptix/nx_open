// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMultiMap>

#include <nx/vms/client/desktop/radass/utils/abstract_timers.h>

namespace nx::vms::client::desktop::test {

/**
* Implements factory to create dummy test timers that progress manually by calling factory methods.
* All timers created by the same factory object progress simultaneously.
*/

class TestTimer;

class TestTimerFactory: public AbstractTimerFactory
{
public:
    using milliseconds = std::chrono::milliseconds;

    virtual AbstractTimer* createTimer(QObject* parent = nullptr) override;
    virtual AbstractElapsedTimer* createElapsedTimer() override;

    // Used by test framework.
    /** @returns current logical time */
    milliseconds currentTime() const;
    /** @returns current logical time */
    bool hasFutureEvents() const;
    milliseconds nextEventTime() const;
    // Next functions do fire timeout() signals.
    void passTime(milliseconds period);
    void advanceToTime(milliseconds timePoint);
    void advanceToNextEvent();

    // Used internally by test timers.
    void registerTimerEvent(milliseconds timePoint, TestTimer* timer);
    void forgetTimer(TestTimer* timer);

private:
    milliseconds m_currentTime = milliseconds();
    QMultiMap<milliseconds, TestTimer*> m_timerEvents;
};

using TestTimerFactoryPtr = QSharedPointer<TestTimerFactory>;

} // namespace nx::vms::client::desktop::test
