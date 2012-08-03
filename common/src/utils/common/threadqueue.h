#ifndef cl_ThreadQueue_h_2236
#define cl_ThreadQueue_h_2236 

#include <QQueue>
#include <QVariant>
#include <QSemaphore>

#include "log.h"

static const qint32 MAX_THREAD_QUEUE_SIZE = 256;

#ifndef INFINITE
#define INFINITE            0xFFFFFFFF  // Infinite timeout
#endif


template <typename T>
class CLThreadQueue
{
public:
    CLThreadQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : m_maxSize( maxSize ),
        m_cs(QMutex::Recursive)
    {
    }
    
    ~CLThreadQueue() {
        clear();
    }

    bool push(const T& val) 
    { 
        QMutexLocker mutex(&m_cs);

        // we can have 2 threads independetlly put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )    return false; <- wrong aproach

        m_queue.enqueue(val); 

        m_sem.release(); 

        return true;
    }

    T front() const
    {
        QMutexLocker mutex(&m_cs);
        return m_queue.front();  
    }

    T at(int i) const
    {
        QMutexLocker mutex(&m_cs);
        return m_queue[i];  
    }

    T last() const
    {
        QMutexLocker mutex(&m_cs);
        return m_queue.last();
    }


    bool pop(T& val, quint32 time = INFINITE)
    {
        if (!m_sem.tryAcquire(1,time)) // in case of INFINITE wait the value passed to tryAcquire will be negative ( it will wait forever )
            return false;

        QMutexLocker mutex(&m_cs);

        if (!m_queue.empty()) 
        {
            val = m_queue.dequeue();  
            return true; 
        }

        return false;
    }

    void lock()
    {
        m_cs.lock();
    }

    void unlock()
    {
        m_cs.unlock();
    }

    void removeAt(int index)
    {
        QMutexLocker mutex(&m_cs);
        m_sem.acquire(1);
        m_queue.removeAt(index);
    }

    template <class ConditionFunc>
    void removeFrontByCondition(const ConditionFunc& cond)
    {
        QMutexLocker mutex(&m_cs);
        while (!m_queue.isEmpty() && cond(m_queue.front())) 
        {
            m_sem.acquire(1);
            m_queue.dequeue();
        }
    }

    template <class ConditionFunc>
    void removeDataByCondition(const ConditionFunc& cond, QVariant opaque)
    {
        QMutexLocker mutex(&m_cs);
        typename QQueue<T>::iterator itr = m_queue.begin();
        while (itr != m_queue.end())
        {
            if (cond(*itr, opaque))
            {
                m_sem.acquire(1);
                itr = m_queue.erase(itr);
            }
            else {
                ++itr;
            }
        }
    }

    int size() const
    {
        QMutexLocker mutex(&m_cs);

        return m_queue.size();
    }

    int maxSize() const
    {
        return m_maxSize;
    }

    void setMaxSize(int value) 
    {
        m_maxSize = value;
    }

    void clear()
    {
        QMutexLocker mutex(&this->m_cs);

        while (!this->m_queue.empty()) 
        {
            this->m_sem.tryAcquire();
            this->m_queue.dequeue();
        }
    }

    void clearDelete()
    {
        QMutexLocker mutex(&this->m_cs);

        while (!this->m_queue.empty()) 
        {
            this->m_sem.tryAcquire();
            delete this->m_queue.dequeue();
        }
    }

protected:
    QQueue<T> m_queue;
    quint32 m_maxSize;
    mutable QMutex m_cs;
    QSemaphore m_sem;
};

#endif //cl_ThreadQueue_h_2236
