#pragma once

#include <atomic>
#include <functional>
#include <set>
#include <tuple>
#include <queue>

#include <boost/optional.hpp>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace utils {

using QueueReaderId = std::size_t;

static const QueueReaderId kInvalidQueueReaderId = (QueueReaderId)-1;

template<typename Result>
class SyncQueue
{
public:
    using ResultType = Result;

    SyncQueue();

    QueueReaderId generateReaderId();

    Result pop();
    boost::optional<Result> pop(QueueReaderId readerId);
    boost::optional<Result> pop(
        std::chrono::milliseconds timeout,
        QueueReaderId readerId = kInvalidQueueReaderId);

    void push(Result result);
    bool isEmpty();
    std::size_t size() const;
    void clear();

    std::function<void(Result)> pusher();

    /**
     * While reader is in terminated status, all pop operations with readerId
     * supplied return immediately without value.
     * WARNING: It is highly recommended to call SyncQueue::removeReaderFromTerminatedList
     * after termination has been completed (e.g., reader thread has stopped) to prevent
     * "terminated reader list" from holding redundant information.
     */
    void addReaderToTerminatedList(QueueReaderId readerId);
    void removeReaderFromTerminatedList(QueueReaderId readerId);

private:
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::queue<Result> m_queue;
    std::set<QueueReaderId> m_terminatedReaders;
    std::atomic<QueueReaderId> m_prevReaderId;
};

template<typename R1, typename R2>
class SyncMultiQueue:
    public SyncQueue<std::pair<R1, R2>>
{
public:
    void push(R1 r1, R2 r2) /* overlap */;
    std::function<void(R1, R2)> pusher() /* overlap */;
};

// --- implementation ---

template<typename Result>
SyncQueue<Result>::SyncQueue():
    m_prevReaderId(0)
{
}

template<typename Result>
QueueReaderId SyncQueue<Result>::generateReaderId()
{
    return ++m_prevReaderId;
}

template< typename Result>
Result SyncQueue<Result>::pop()
{
    auto value = pop(std::chrono::milliseconds(0));
    NX_ASSERT(value);
    return std::move(*value);
}

template< typename Result>
boost::optional<Result> SyncQueue<Result>::pop(QueueReaderId readerId)
{
    return pop(std::chrono::milliseconds(0), readerId);
}

/**
 * @param timeout std::chrono::milliseconds::zero() means "no timeout"
 */
template< typename Result>
boost::optional<Result> SyncQueue<Result>::pop(
    std::chrono::milliseconds timeout,
    QueueReaderId readerId)
{
    using namespace std::chrono;

    QnMutexLocker lock(&m_mutex);

    boost::optional<steady_clock::time_point> deadline;
    if (timeout.count())
        deadline = steady_clock::now() + timeout;
    while (m_queue.empty())
    {
        if (m_terminatedReaders.find(readerId) != m_terminatedReaders.end())
            return boost::none;

        if (deadline)
        {
            const auto currentTime = steady_clock::now();
            if (currentTime >= *deadline)
                return boost::none;
            if (!m_condition.wait(
                    &m_mutex,
                    duration_cast<milliseconds>(*deadline-currentTime).count()))
            {
                return boost::none;
            }
        }
        else
        {
            m_condition.wait(&m_mutex);
        }
    }

    if (m_terminatedReaders.find(readerId) != m_terminatedReaders.end())
        return boost::none;

    auto result = std::move(m_queue.front());
    m_queue.pop();
    return std::move(result);
}

template< typename Result>
void SyncQueue<Result>::push(Result result)
{
    QnMutexLocker lock( &m_mutex );
    const auto wasEmpty = m_queue.empty();

    m_queue.push( std::move( result ) );
    if( wasEmpty )
        m_condition.wakeOne();
}

template< typename Result>
bool SyncQueue<Result>::isEmpty()
{
    QnMutexLocker lock( &m_mutex );
    return m_queue.empty();
}

template< typename Result>
std::size_t SyncQueue<Result>::size() const
{
    QnMutexLocker lock( &m_mutex );
    return m_queue.size();
}

template< typename Result>
void SyncQueue<Result>::clear()
{
    QnMutexLocker lock( &m_mutex );
    decltype( m_queue ) queue;
    std::swap( m_queue, queue );
}

template<typename Result>
std::function<void(Result) > SyncQueue<Result>::pusher()
{
    return [this](Result result) { push(std::move(result)); };
}

template<typename Result>
void SyncQueue<Result>::addReaderToTerminatedList(QueueReaderId readerId)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminatedReaders.insert(readerId);
    }
    m_condition.wakeAll();
}

template<typename Result>
void SyncQueue<Result>::removeReaderFromTerminatedList(QueueReaderId readerId)
{
    QnMutexLocker lock(&m_mutex);
    m_terminatedReaders.erase(readerId);
}

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
