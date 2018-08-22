#pragma once

#include <QtCore/QObject>

#include "thread/mutex.h"
#include "thread/wait_condition.h"

namespace nx {
namespace utils {

class NX_UTILS_API Counter:
    public QObject
{
    Q_OBJECT;

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

    Counter(int initialCount = 0, QObject* parent = NULL);

    /**
     * Increments internal counter.
     * Counter will be decremented when returned ScopedIncrement is destroyed.
     */
    ScopedIncrement getScopedIncrement();
    /**
     * Blocks until internal counter reaches zero.
     */
    void wait();

signals:
    void reachedZero();

public slots:
    void increment();
    void decrement();

private:
    QnMutex m_mutex;
    QnWaitCondition m_counterReachedZeroCondition;
    int m_count;
};

} // namespace utils
} // namespace nx
