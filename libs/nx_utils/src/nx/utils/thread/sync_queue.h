// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <algorithm>
#include <deque>
#include <functional>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace utils {

using QueueReaderId = std::size_t;

static const QueueReaderId kInvalidQueueReaderId = (QueueReaderId)-1;

template<typename Result>
class SyncQueueBase
{
public:
    using ResultType = Result;

    using OptionalResultType = std::conditional_t<
        std::is_same_v<Result, void>,
        bool,
        std::optional<Result>>;

    SyncQueueBase();

    QueueReaderId generateReaderId();

    ResultType pop();

    OptionalResultType pop(QueueReaderId readerId);

    OptionalResultType pop(
        std::optional<std::chrono::milliseconds> timeout,
        QueueReaderId readerId = kInvalidQueueReaderId);

    template<typename ConditionFunc>
    OptionalResultType popIf(
        ConditionFunc conditionFunc,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt,
        QueueReaderId readerId = kInvalidQueueReaderId);

    bool isEmpty();
    bool empty() const;
    std::size_t size() const;
    void clear();
    void retestPopIfCondition();

    /**
     * While reader is in terminated status, all pop operations with readerId
     * supplied return immediately without value.
     * WARNING: It is highly recommended to call SyncQueue::removeReaderFromTerminatedList
     * after termination has been completed (e.g., reader thread has stopped) to prevent
     * "terminated reader list" from holding redundant information.
     */
    void addReaderToTerminatedList(QueueReaderId readerId);
    void removeReaderFromTerminatedList(QueueReaderId readerId);

protected:
    using StorageType = std::conditional_t<
        std::is_same_v<Result, void>,
        bool, // dummy type
        Result>;

    mutable nx::Mutex m_mutex;
    nx::WaitCondition m_condition;
    std::deque<StorageType> m_queue;
    std::set<QueueReaderId> m_terminatedReaders;
    std::atomic<QueueReaderId> m_prevReaderId;

    bool waitForNonEmptyQueue(
        nx::Locker<nx::Mutex>* lock,
        std::optional<std::chrono::steady_clock::time_point> deadline,
        QueueReaderId readerId);

    bool waitForEvent(
        nx::Locker<nx::Mutex>* lock,
        std::optional<std::chrono::steady_clock::time_point> deadline);
};

//-------------------------------------------------------------------------------------------------

template<typename Result>
class SyncQueue:
    public SyncQueueBase<Result>
{
    using base_type = SyncQueueBase<Result>;

public:
    using base_type::base_type;

    void push(Result value);

    /**
     * Constructs Result in-place.
     */
    template<typename... Args>
    void emplace(Args&&... args);

    std::function<void(Result)> pusher();
};

//-------------------------------------------------------------------------------------------------

template<>
class SyncQueue<void>:
    public SyncQueueBase<void>
{
    using base_type = SyncQueueBase<void>;

public:
    using base_type::base_type;

    void push()
    {
        NX_MUTEX_LOCKER lock(&this->m_mutex);
        const auto wasEmpty = this->m_queue.empty();

        this->m_queue.push_back(/*dummy*/ true);
        if (wasEmpty)
            this->m_condition.wakeOne();
    }

    std::function<void()> pusher()
    {
        return [this]() { push(); };
    }
};

//-------------------------------------------------------------------------------------------------

template<typename R1, typename R2>
class SyncMultiQueue:
    public SyncQueue<std::pair<R1, R2>>
{
public:
    void push(R1 r1, R2 r2) /* overlap */;
    std::function<void(R1, R2)> pusher() /* overlap */;
};

//-------------------------------------------------------------------------------------------------
// Implementation.

template<typename Result>
SyncQueueBase<Result>::SyncQueueBase():
    m_prevReaderId(0)
{
}

template<typename Result>
QueueReaderId SyncQueueBase<Result>::generateReaderId()
{
    return ++m_prevReaderId;
}

template< typename Result>
Result SyncQueueBase<Result>::pop()
{
    auto value = pop(std::nullopt);
    NX_ASSERT(value);

    if constexpr (!std::is_same_v<Result, void>)
    {
        auto res = std::move(*value);
        return res;
    }
    else
        return;
}

template< typename Result>
typename SyncQueueBase<Result>::OptionalResultType SyncQueueBase<Result>::pop(QueueReaderId readerId)
{
    return pop(std::nullopt, readerId);
}

template< typename Result>
typename SyncQueueBase<Result>::OptionalResultType SyncQueueBase<Result>::pop(
    std::optional<std::chrono::milliseconds> timeout,
    QueueReaderId readerId)
{
    return popIf([](const StorageType&) { return true; }, timeout, readerId);
}

template<typename Result>
template<typename ConditionFunc>
typename SyncQueueBase<Result>::OptionalResultType SyncQueueBase<Result>::popIf(
    ConditionFunc conditionFunc,
    std::optional<std::chrono::milliseconds> timeout,
    QueueReaderId readerId)
{
    using namespace std::chrono;

    NX_MUTEX_LOCKER lock(&m_mutex);

    std::optional<steady_clock::time_point> deadline;
    if (timeout)
        deadline = steady_clock::now() + *timeout;

    for (;;)
    {
        if (!waitForNonEmptyQueue(&lock, deadline, readerId))
            return OptionalResultType();

        auto resultIter = std::find_if(m_queue.begin(), m_queue.end(), conditionFunc);
        if (resultIter != m_queue.end())
        {
            auto result = std::move(*resultIter);
            m_queue.erase(resultIter);
            return result;
        }

        // Could not find satisfying element - waiting for an event or for a new element.
        if (!waitForEvent(&lock, deadline))
            return OptionalResultType(); // std::nullopt or false.
    }
}

template< typename Result>
bool SyncQueueBase<Result>::isEmpty()
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    return m_queue.empty();
}

template< typename Result>
bool SyncQueueBase<Result>::empty() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_queue.empty();
}

template< typename Result>
std::size_t SyncQueueBase<Result>::size() const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    return m_queue.size();
}

template< typename Result>
void SyncQueueBase<Result>::clear()
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    decltype( m_queue ) queue;
    std::swap( m_queue, queue );
}

template< typename Result>
void SyncQueueBase<Result>::retestPopIfCondition()
{
    // Mutex is required here to be sure popIf does not miss notification.
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_condition.wakeAll();
}

template<typename Result>
void SyncQueueBase<Result>::addReaderToTerminatedList(QueueReaderId readerId)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_terminatedReaders.insert(readerId);
    }
    m_condition.wakeAll();
}

template<typename Result>
void SyncQueueBase<Result>::removeReaderFromTerminatedList(QueueReaderId readerId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_terminatedReaders.erase(readerId);
}

template<typename Result>
bool SyncQueueBase<Result>::waitForNonEmptyQueue(
    nx::Locker<nx::Mutex>* lock,
    std::optional<std::chrono::steady_clock::time_point> deadline,
    QueueReaderId readerId)
{
    while (m_queue.empty())
    {
        if (m_terminatedReaders.find(readerId) != m_terminatedReaders.end())
            return false;

        if (!waitForEvent(lock, deadline))
            return false;
    }

    if (m_terminatedReaders.find(readerId) != m_terminatedReaders.end())
        return false;

    return true;
}

template< typename Result>
bool SyncQueueBase<Result>::waitForEvent(
    nx::Locker<nx::Mutex>* lock,
    std::optional<std::chrono::steady_clock::time_point> deadline)
{
    using namespace std::chrono;

    if (deadline)
    {
        const auto currentTime = steady_clock::now();
        if (currentTime >= *deadline)
            return false;
        if (!m_condition.wait(
                lock->mutex(),
                duration_cast<milliseconds>(*deadline - currentTime).count()))
        {
            return false;
        }
    }
    else
    {
        m_condition.wait(lock->mutex());
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

template<typename Result>
void SyncQueue<Result>::push(Result value)
{
    NX_MUTEX_LOCKER lock(&this->m_mutex);
    const auto wasEmpty = this->m_queue.empty();

    this->m_queue.push_back(std::move(value));
    if (wasEmpty)
        this->m_condition.wakeOne();
}

template<typename Result>
template<typename... Args>
void SyncQueue<Result>::emplace(Args&&... args)
{
    NX_MUTEX_LOCKER lock(&this->m_mutex);
    const auto wasEmpty = this->m_queue.empty();

    this->m_queue.emplace_back(std::forward<Args>(args)...);
    if (wasEmpty)
        this->m_condition.wakeOne();
}

template<typename Result>
std::function<void(Result)> SyncQueue<Result>::pusher()
{
    return [this](Result result) { push(std::move(result)); };
}

//-------------------------------------------------------------------------------------------------

template<typename R1, typename R2>
std::function<void(R1, R2)> SyncMultiQueue<R1, R2>::pusher()
{
    return [this](R1 r1, R2 r2) { push(std::move(r1), std::move(r2)); };
}

template<typename R1, typename R2>
void SyncMultiQueue<R1, R2>::push(R1 r1, R2 r2)
{
    SyncQueue<std::pair<R1, R2>>::push(std::make_pair(std::move(r1), std::move(r2)));
}

} // namespace utils
} // namespace nx
