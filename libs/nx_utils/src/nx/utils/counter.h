// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "thread/mutex.h"
#include "thread/wait_condition.h"

namespace nx {
namespace utils {

class NX_UTILS_API Counter
{
public:
    class NX_UTILS_API ScopedIncrement
    {
    public:
        ScopedIncrement() = default;
        ScopedIncrement(Counter* const counter);
        ScopedIncrement(ScopedIncrement&& right);
        ScopedIncrement(const ScopedIncrement& right);

        ScopedIncrement& operator=(ScopedIncrement&& right);

        ~ScopedIncrement();

    private:
        Counter* m_counter = nullptr;
    };

    Counter(int initialCount = 0);
    virtual ~Counter() = default;

    /**
     * Increments internal counter.
     * Counter will be decremented when returned ScopedIncrement is destroyed.
     */
    ScopedIncrement getScopedIncrement();

    /**
     * Blocks until internal counter reaches zero.
     */
    void wait();

    /**
     * Blocks until internal counter reaches zero or timeout passes.
     * @return true if counter has reached zero. false if timeout has passed.
     */
    bool waitFor(std::chrono::milliseconds timeout);

    /**
     * @return New counter value.
     */
    int increment();

    /**
     * @return New counter value.
     */
    virtual int decrement();

    /**
     * Provides current counter value. Usually, this value is of not much use, because it can be
     * changed concurrently. But, it may still be useful to compare it against zero in some cases.
     * @return Current value.
     */
    int value() const;

private:
    mutable nx::Mutex m_mutex;
    nx::WaitCondition m_counterReachedZeroCondition;
    int m_count;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API CounterWithSignal:
    public QObject,
    public Counter
{
    Q_OBJECT

    using base_type = Counter;

public:
    CounterWithSignal(int initialCount = 0, QObject* parent = NULL);

public slots:
    virtual int decrement() override;

signals:
    void reachedZero();
};

} // namespace utils
} // namespace nx
