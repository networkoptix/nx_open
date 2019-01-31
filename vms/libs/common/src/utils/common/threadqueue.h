#ifndef cl_ThreadQueue_h_2236
#define cl_ThreadQueue_h_2236

#include <vector>

#include <QtCore/QQueue>
#include <QtCore/QVariant>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/semaphore.h>


static const qint32 MAX_THREAD_QUEUE_SIZE = 256;

template <typename T>
class QnSafeQueue
{
public:

    template <class Q=QnSafeQueue>
    class RandomAccess
    {
    public:
        RandomAccess<Q>(const RandomAccess<Q>&) = delete;
        RandomAccess<Q>& operator=(const RandomAccess<Q>&) = delete;

        RandomAccess<Q>(RandomAccess<Q>&& other)
        {
            m_q = other.m_q;
            other.m_q = nullptr;
        }
        RandomAccess<Q>& operator=(RandomAccess<Q>&& other)
        {
            m_q = other.m_q;
            other.m_q = nullptr;
        }

        RandomAccess<Q>(Q* queue):
            m_q(queue)
        {
            m_q->lockInternal();
        }

        ~RandomAccess<Q>()
        {
            if (m_q)
                m_q->unlockInternal();
        }

        const T& at(int i) const
        {
            return m_q->atUnsafe(i);
        }

        void setAt(const T& value, int i)
        {
            int index = m_q->m_headIndex + i;
            m_q->m_buffer[index % m_q->m_buffer.size()] = value;
        }

        void removeAt(int index)
        {
            m_q->removeAtUnsafe(index);
        }

        void popFront()
        {
            m_q->m_buffer[m_q->m_headIndex++] = T();
            if (m_q->m_headIndex >= (int) m_q->m_buffer.size())
                m_q->m_headIndex = 0;
            --m_q->m_bufferLen;
        }

        T front() const
        {
            return m_q->m_buffer[m_q->m_headIndex];
        }

        T last() const
        {
            int index = m_q->m_headIndex + m_q->m_bufferLen - 1;
            return m_q->m_bufferLen > 0 ? m_q->m_buffer[index % m_q->m_buffer.size()] : T();
        }

        int size() const
        {
            return m_q->size();
        }

    private:
        Q* m_q = nullptr;
    };

    QnSafeQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : m_headIndex(0),
        m_bufferLen(0),
        m_maxSize(maxSize),
        m_terminated(false)
    {
        reallocateBufferUnsafe(maxSize);
    }

    ~QnSafeQueue()
    {
        clearUnsafe();
    }

    RandomAccess<const QnSafeQueue> lock() const
    {
        return RandomAccess<const QnSafeQueue>(const_cast<QnSafeQueue*>(this));
    }

    RandomAccess<> lock()
    {
        return RandomAccess<>(this);
    }


    bool isEmpty() const
    {
        return m_bufferLen == 0;
    }

    template<typename ValueRef>
    bool push(ValueRef&& val)
    {
        QnMutexLocker mutex(&m_mutex);

        if ((uint)m_bufferLen == m_buffer.size())
            reallocateBufferUnsafe(qMax(m_bufferLen + 1, m_bufferLen + m_bufferLen/4));

        // we can have 2 threads independetlly put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )    return false; <- wrong aproach

        int index = (m_headIndex + m_bufferLen) % m_buffer.size();
        m_buffer[index] = std::forward<ValueRef>(val);
        m_bufferLen++;

        m_waitCond.wakeOne();

        return true;
    }

    bool pop(T& val, unsigned long time = ULONG_MAX)
    {
        QnMutexLocker mutex(&m_mutex);
        if (!m_terminated && m_bufferLen == 0)
            m_waitCond.wait(&m_mutex, time);
        if (m_bufferLen == 0)
            return false;

        val = std::move(m_buffer[m_headIndex]);
        m_headIndex++;
        if ((uint)m_headIndex >= m_buffer.size())
            m_headIndex = 0;
        m_bufferLen--;
        return true;
    }

    template <class ConditionFunc>
    void detachDataByCondition(const ConditionFunc& cond, const QVariant& opaque = QVariant())
    {
        QnMutexLocker mutex(&m_mutex);
        int index = m_headIndex;
        for (int i = 0; i < m_bufferLen; ++i)
        {
            if (cond(m_buffer[index], opaque))
                m_buffer[index] = T();
            index = (index + 1) % m_buffer.size();
        }
    }

    int size() const
    {
        return m_bufferLen;
    }

    int maxSize() const
    {
        return m_maxSize;
    }

    void setMaxSize(int value)
    {
        QnMutexLocker mutex(&m_mutex);
        m_maxSize = value;
        reallocateBufferUnsafe(qMax(m_maxSize, (int) m_buffer.size()));
    }

    void clear()
    {
        QnMutexLocker lock(&m_mutex);
        clearUnsafe();
        m_waitCond.wakeOne();
    }

    /**
     * Queue in terminated mode will not wait for new data if empty. But pop call returns already queued data anyway.
     */
    void setTerminated(bool value)
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = value;
        m_waitCond.wakeAll();
    }

private:

    void lockInternal() const
    {
        m_mutex.lock();
    }

    void unlockInternal() const
    {
        m_mutex.unlock();
    }

    const T& atUnsafe(int i) const
    {
        int index = m_headIndex + i;
        return m_buffer[index % m_buffer.size()];
    }

    void removeAtUnsafe(int index)
    {
        int bufferIndex = (m_headIndex + index) % m_buffer.size();
        int toMove = m_bufferLen - index - 1;
        for (int i = 0; i < toMove; ++i)
        {
            int nextIndex = (bufferIndex + 1) % m_buffer.size();
            m_buffer[bufferIndex] = std::move(m_buffer[nextIndex]);
            bufferIndex = nextIndex;
        }
        m_buffer[bufferIndex] = T();
        --m_bufferLen;
    }

private:
    // for grow only
    void reallocateBufferUnsafe(int newSize)
    {
        int oldSize = (int) m_buffer.size();
        m_buffer.resize(newSize);

        if (m_headIndex > 0 && m_bufferLen > 0 && newSize > oldSize)
        {
            int tailIndex = m_headIndex + m_bufferLen;
            if (tailIndex > oldSize)
                tailIndex -= oldSize;
            else
                return; // no correction is needed

            int delta = newSize-oldSize;

            for (int i = 0; i < delta && i < tailIndex; ++i)
                m_buffer[oldSize + i] = std::move(m_buffer[i]);
            int i = 0;
            for (;i < tailIndex - delta; ++i)
                m_buffer[i] = std::move(m_buffer[i+delta]);
            for (;i < tailIndex; ++i)
                m_buffer[i] = T();
        }
    }

    void clearUnsafe()
    {
        int index = m_headIndex;
        for (int i = 0; i < m_bufferLen; ++i)
        {
            m_buffer[index] = T();
            index = (index + 1) % m_buffer.size();
        }
        m_bufferLen = 0;
        m_headIndex = 0;
    }

protected:
    std::vector<T> m_buffer;
    int m_headIndex;
    int m_bufferLen;

    int m_maxSize;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_waitCond;
    bool m_terminated;
};

template <typename T>
class QnUnsafeQueue
{
public:
    QnUnsafeQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : m_headIndex(0),
        m_bufferLen(0),
        m_maxSize( maxSize )
    {
        reallocateBuffer(maxSize);
    }

    ~QnUnsafeQueue() {
        clear();
    }

    bool isEmpty() const
    {
        return m_bufferLen == 0;
    }

    bool push(const T& val)
    {
        if ((uint)m_bufferLen == m_buffer.size())
            reallocateBuffer(qMax(m_bufferLen + 1, m_bufferLen + m_bufferLen/4));

        // we can have 2 threads independetlly put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )    return false; <- wrong aproach

        //m_queue.enqueue(val);
        int index = (m_headIndex + m_bufferLen) % m_buffer.size();
        m_buffer[index] = val;
        m_bufferLen++;

        return true;
    }

    T front() const
    {
        return m_buffer[m_headIndex];
    }

    const T& at(int i) const
    {
        int index = m_headIndex + i;
        return m_buffer[index % m_buffer.size()];
    }

    T last() const
    {
        int index = m_headIndex + m_bufferLen - 1;
        return m_buffer[index % m_buffer.size()];
    }


    bool pop(T& val)
    {
        if (m_bufferLen > 0)
        {
            val = std::move(m_buffer[m_headIndex]);
            m_headIndex++;
            if ((uint)m_headIndex >= m_buffer.size())
                m_headIndex = 0;
            m_bufferLen--;
            return true;
        }
        return false;
    }

    template <class ConditionFunc>
    void detachDataByCondition(const ConditionFunc& cond, QVariant opaque)
    {
        int index = m_headIndex;
        for (int i = 0; i < m_bufferLen; ++i)
        {
            if (cond(m_buffer[index], opaque))
                m_buffer[index] = T();
            index = (index + 1) % m_buffer.size();
        }
    }

    int size() const
    {
        return m_bufferLen;
    }

    int maxSize() const
    {
        return m_maxSize;
    }

    void setMaxSize(int value)
    {
        m_maxSize = value;
        reallocateBuffer(qMax(m_maxSize, (int) m_buffer.size()));
    }

    void clear()
    {
        int index = m_headIndex;
        for (int i = 0; i < m_bufferLen; ++i)
        {
            m_buffer[index] = T();
            index = (index + 1) % m_buffer.size();
        }
        m_bufferLen = 0;
        m_headIndex = 0;
    }

private:
    // for grow only
    void reallocateBuffer(int newSize)
    {
        int oldSize = (int) m_buffer.size();
        m_buffer.resize(newSize);

        if (m_headIndex > 0 && m_bufferLen > 0 && newSize > oldSize)
        {
            int tailIndex = m_headIndex + m_bufferLen;
            if (tailIndex > oldSize)
                tailIndex -= oldSize;
            else
                return; // no correction is needed

            int delta = newSize-oldSize;

            for (int i = 0; i < delta && i < tailIndex; ++i)
                m_buffer[oldSize + i] = m_buffer[i];
            int i = 0;
            for (;i < tailIndex - delta; ++i)
                m_buffer[i] = m_buffer[i+delta];
            for (;i < tailIndex; ++i)
                m_buffer[i] = T();
        }
    }

protected:
    std::vector<T> m_buffer;
    int m_headIndex;
    int m_bufferLen;

    int m_maxSize;
};

#endif //cl_ThreadQueue_h_2236
