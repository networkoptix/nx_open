#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <queue>
#include <tuple>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace utils {

template<typename Result>
class SyncQueue
{
public:
    typedef Result ResultType;

    Result pop();
    boost::optional<Result> pop(std::chrono::milliseconds timeout);

    void push(Result result);
    bool isEmpty();
    std::size_t size() const;

    std::function<void(Result)> pusher();

private:
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::queue<Result> m_queue;
};

template<typename R1, typename R2>
class SyncMultiQueue
:
    public SyncQueue<std::pair<R1, R2>>
{
public:
    void push(R1 r1, R2 r2) /* overlap */;
    std::function<void(R1, R2)> pusher() /* overlap */;
};

// --- implementation ---

template< typename Result>
Result SyncQueue<Result>::pop()
{
    auto value = pop(std::chrono::milliseconds(0));
    NX_ASSERT(value);
    return std::move(*value);
}

/**
 * @param timeout std::chrono::milliseconds::zero() means "no timeout"
 */
template< typename Result>
boost::optional<Result> SyncQueue<Result>::pop(std::chrono::milliseconds timeout)
{
    using namespace std::chrono;

    QnMutexLocker lock(&m_mutex);

    boost::optional<steady_clock::time_point> deadline;
    if (timeout.count())
        deadline = steady_clock::now() + timeout;
    while (m_queue.empty())
    {
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

template<typename Result>
std::function<void(Result) > SyncQueue<Result>::pusher()
{
    return [this](Result result) { push(std::move(result)); };
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
