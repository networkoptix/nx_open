////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <queue>

#include <QMutex>
#include <QWaitCondition>


template<class T>
    class BlockingQueue
{
public:
    typedef typename std::queue<T>::size_type size_type;

    //!If queue is empty, blocks till queue has some elements
    T& front()
    {
        QMutexLocker lk( &m_mutex );
        while( m_data.empty() )
            m_cond.wait( lk.mutex() );
        return m_data.front();
    }

    //!Removes top element. Does not check for queue emptiness
    void pop()
    {
        QMutexLocker lk( &m_mutex );
        return m_data.pop();
    }

    bool push( const T& val )
    {
        QMutexLocker lk( &m_mutex );
        m_data.push( val );
        m_cond.wakeAll();
        return true;
    }

    bool empty() const
    {
        QMutexLocker lk( &m_mutex );
        return m_data.empty();
    }

private:
    std::queue<T> m_data;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
};

#endif  //BLOCKING_QUEUE_H
