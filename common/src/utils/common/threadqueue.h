#ifndef cl_ThreadQueue_h_2236
#define cl_ThreadQueue_h_2236

#include <vector>

#include <QtCore/QQueue>
#include <QtCore/QVariant>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/semaphore.h>


static const qint32 MAX_THREAD_QUEUE_SIZE = 256;

#ifndef INFINITE
#define INFINITE            0xFFFFFFFF  // Infinite timeout
#endif


template <typename T>
class CLThreadQueue
{
public:
    CLThreadQueue( quint32 maxSize = MAX_THREAD_QUEUE_SIZE)
        : m_headIndex(0),
        m_bufferLen(0),
        m_maxSize( maxSize )
    {
        reallocateBufferUnsafe(maxSize);
    }

    ~CLThreadQueue() {
        clear();
    }

    bool isEmpty() const
    {
        return m_bufferLen == 0;
    }

    bool contains(const T& val) const
    {
        QnMutexLocker lock(&m_mutex);
        for (int i = 0; i < m_bufferLen; ++i)
        {
            if (atUnsafe(i) == val)
                return true;
        }
        return false;
    }

    template<typename ValueRef>
    bool push(ValueRef&& val)
    {
        QnMutexLocker lock(&m_mutex);
        if ((uint)m_bufferLen == m_buffer.size())
            reallocateBufferUnsafe(qMax(m_bufferLen + 1, m_bufferLen + m_bufferLen/4));


        // we can have 2 threads independetlly put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )    return false; <- wrong aproach

        //m_queue.enqueue(val);
        int index = (m_headIndex + m_bufferLen) % m_buffer.size();
        m_buffer[index] = std::forward<ValueRef>(val);
        m_bufferLen++;

        m_waitCond.wakeAll();

        return true;
    }

    T front() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_buffer[m_headIndex];
    }

    const T& at(int i) const
    {
        QnMutexLocker lock(&m_mutex);
        int index = m_headIndex + i;
        return m_buffer[index % m_buffer.size()];
    }

    const T& atUnsafe(int i) const
    {
        int index = m_headIndex + i;
        return m_buffer[index % m_buffer.size()];
    }

    void setAt(const T& value, int i)
    {
        QnMutexLocker lock(&m_mutex);
        int index = m_headIndex + i;
        m_buffer[index % m_buffer.size()] = value;
    }

    T last() const
    {
        QnMutexLocker lock(&m_mutex);
        int index = m_headIndex + m_bufferLen - 1;
        return m_bufferLen > 0 ? m_buffer[index % m_buffer.size()]: T();
    }


    bool pop(T& val, quint32 time = ULONG_MAX)
    {
        QnMutexLocker lock(&m_mutex);
        if (m_bufferLen == 0)
            m_waitCond.wait(&m_mutex, time);

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

    void removeFirst(int count)
    {
        QnMutexLocker lock(&m_mutex);
        for (int i = 0; i < count; ++i)
        {
            m_buffer[m_headIndex++] = T();
            if (m_headIndex >= (int)m_buffer.size())
                m_headIndex = 0;
        }
        m_bufferLen -= count;
    }

    void removeAt(int index)
    {
        QnMutexLocker lock(&m_mutex);
        removeAtUnsafe(index);
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

    void lock() const
    {
        m_mutex.lock();
    }

    void unlock() const
    {
        m_mutex.unlock();
    }

    template <class ConditionFunc>
    void detachDataByCondition(const ConditionFunc& cond, const QVariant& opaque = QVariant())
    {
        QnMutexLocker lock(&m_mutex);
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
        QnMutexLocker lock(&m_mutex);
        m_maxSize = value;
        reallocateBufferUnsafe(qMax(m_maxSize, (int) m_buffer.size()));
    }

    void clear()
    {
        QnMutexLocker lock(&m_mutex);

        int index = m_headIndex;
        for (int i = 0; i < m_bufferLen; ++i)
        {
            m_buffer[index] = T();
            index = (index + 1) % m_buffer.size();
        }
        m_bufferLen = 0;
        m_headIndex = 0;
    }

    void wakeAll()
    {
        m_waitCond.wakeAll();
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

protected:
    std::vector<T> m_buffer;
    int m_headIndex;
    std::atomic<int> m_bufferLen;

    int m_maxSize;
    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCond;
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
        reallocateBufferUnsafe(maxSize);
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
            reallocateBufferUnsafe(qMax(m_bufferLen + 1, m_bufferLen + m_bufferLen/4));

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

    void removeFirst(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            m_buffer[m_headIndex++] = T();
            if (m_headIndex >= m_buffer.size())
                m_headIndex = 0;
        }
        m_bufferLen -= count;
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
        reallocateBufferUnsafe(qMax(m_maxSize, (int) m_buffer.size()));
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
