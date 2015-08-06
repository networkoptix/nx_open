#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <queue>

#include "mutex.h"
#include "wait_condition.h"

namespace nx {

template< typename Result>
class SyncQueue
{
public:
    Result pop();
    void push(Result result);

    std::function< void( Result ) > pusher();

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::queue<Result> m_queue;
};

// TODO: replace with variadic template
template< typename R1, typename R2 >
class SyncMultiQueue
        : public SyncQueue< std::pair< R1, R2 > >
{
public:
    std::function< void( R1, R2 ) > pusher() /* overlap */;
};

// --- implementation ---

template< typename Result>
Result SyncQueue<Result>::pop()
{
    QnMutexLocker lock( &m_mutex );
    while( m_queue.empty() )
        m_condition.wait( &m_mutex );

    auto result = std::move( m_queue.front() );
    m_queue.pop();
    return std::move( result );
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

} // namespace nx

#endif // SYNC_QUEUE_H
