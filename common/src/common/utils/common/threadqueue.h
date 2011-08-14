#ifndef cl_ThreadQueue_h_2236
#define cl_ThreadQueue_h_2236

#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QSemaphore>

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

	void clear()
	{
		QMutexLocker mutex(&m_cs);

		while (!m_queue.empty())
		{
			m_sem.tryAcquire();
			m_queue.dequeue();
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

	~CLThreadQueue()
	{
		clear();
	}

private:
	QQueue<T> m_queue;
	quint32 m_maxSize;
	mutable QMutex m_cs;
	QSemaphore m_sem;
};

#endif //cl_ThreadQueue_h_2236
