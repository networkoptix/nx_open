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
     * @return New counter value.
     */
    int increment();
    
    /**
     * @return New counter value.
     */
    virtual int decrement();

private:
    QnMutex m_mutex;
    QnWaitCondition m_counterReachedZeroCondition;
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
