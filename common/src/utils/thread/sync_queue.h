#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <functional>
#include <queue>
#include <boost/optional.hpp>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>


namespace nx {

template<typename Result>
class SyncQueue
{
public:
    typedef Result ResultType;

    Result pop();
    boost::optional<Result> pop(std::chrono::milliseconds timeout);

    void push(Result result);
    bool isEmpty();

    std::function<void(Result)> pusher();

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::queue<Result> m_queue;
};

// TODO: replace with variadic template
template< typename R1, typename R2 >
class SyncMultiQueue
:
    public SyncQueue< std::pair< R1, R2 > >
{
public:
    std::function< void( R1, R2 ) > pusher() /* overlap */;
};

// --- implementation ---

template< typename Result>
Result SyncQueue<Result>::pop()
{
    auto value = pop(std::chrono::milliseconds(0));
    NX_ASSERT(value);
    return std::move(*value);
}

template< typename Result>
boost::optional<Result> SyncQueue<Result>::pop(std::chrono::milliseconds timeout)
{
    QnMutexLocker lock(&m_mutex);
    if (m_queue.empty()) // no false positive in QWaitCondition
    {
        if (timeout.count() != 0)
        {
            if (!m_condition.wait(&m_mutex, timeout.count()))
                return boost::none;
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
std::function< void( Result ) > SyncQueue<Result>::pusher()
{
    return [ this ]( Result result ) { push( std::move( result ) ); };
}

template< typename R1, typename R2 >
std::function< void( R1, R2 ) > SyncMultiQueue<R1, R2>::pusher()
{
    return [ this ]( R1 r1, R2 r2 )
    {
        this->push( std::make_pair( std::move(r1), std::move(r2) ) );
    };
}

// --- test features ---

// Every test MUST be finite!
static const std::chrono::minutes kTestSyncQueueTimeout(1);

template<typename Base>
class TestSyncQueueBase
:
    public Base
{
public:
    explicit TestSyncQueueBase(
        std::chrono::milliseconds timeout = kTestSyncQueueTimeout)
    :
        m_timeout(timeout)
    {
    }

    typename Base::ResultType pop() /* overlap */
    {
        auto value = Base::pop(m_timeout);
        NX_ASSERT(value, "TestSyncQueue::pop() timeout");
        return std::move(*value);
    }

private:
    std::chrono::milliseconds m_timeout;
};

template<typename Result>
using TestSyncQueue = TestSyncQueueBase<SyncQueue<Result>>;

template<typename R1, typename R2>
using TestSyncMultiQueue = TestSyncQueueBase<SyncMultiQueue<R1, R2>>;

} // namespace nx

#endif // SYNC_QUEUE_H
