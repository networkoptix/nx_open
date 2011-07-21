#ifndef cl_ThreadQueue_h_2236
#define cl_ThreadQueue_h_2236 

#include "log.h"

static const qint32 MAX_THREAD_QUEUE_SIZE = 256;

#ifndef INFINITE
#define INFINITE            0xFFFFFFFF  // Infinite timeout
#endif

template <typename T>
class CLNonReferredThreadQueue
{
public:
    CLNonReferredThreadQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : m_maxSize( maxSize )
    {
    }
    bool push(const T& val) 
    { 
        QMutexLocker mutex(&m_cs);

        // we can have 2 threads independetlly put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )	return false; <- wrong aproach

        m_queue.enqueue(val); 

        m_sem.release(); 

        return true;
    }

    T front()
    {
        QMutexLocker mutex(&m_cs);
        return m_queue.front();  
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
protected:
    QQueue<T> m_queue;
    quint32 m_maxSize;
    mutable QMutex m_cs;
    QSemaphore m_sem;
};

template <typename T>
class CLThreadQueue: public CLNonReferredThreadQueue<T>
{
public:
	CLThreadQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : CLNonReferredThreadQueue<T>( maxSize )
	{
        this->maxSize();
	}

	void clear()
	{
    	QMutexLocker mutex(&this->m_cs);

		while (!this->m_queue.empty()) 
		{
			this->m_sem.tryAcquire();
			this->m_queue.dequeue()->releaseRef();
		}
	}

	~CLThreadQueue()
	{
		clear();
	}
};

#endif //cl_ThreadQueue_h_2236
