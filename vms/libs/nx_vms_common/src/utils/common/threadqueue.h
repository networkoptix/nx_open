// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>
#include <vector>

#include <QtCore/QQueue>
#include <QtCore/QVariant>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/utils/thread/wait_condition.h>

static constexpr qint32 kDefaultMaxThreadQueueSize = 256;

template <typename T>
class QnSafeQueue
{
public:
    template <class Q = QnSafeQueue>
    class RandomAccess
    {
    public:
        RandomAccess(const RandomAccess<Q>&) = delete;
        RandomAccess<Q>& operator=(const RandomAccess<Q>&) = delete;

        RandomAccess(RandomAccess<Q>&& other) noexcept
        {
            m_q = std::exchange(other.m_q, nullptr);
        }
        RandomAccess<Q>& operator=(RandomAccess<Q>&& other) noexcept
        {
            m_q = std::exchange(other.m_q, nullptr);
            return *this;
        }

        ~RandomAccess()
        {
            if (m_q)
                m_q->unlockInternal();
        }

        // Returns T() in case of invalid index.
        const T& at(int index) const
        {
            return m_q->atUnsafe(index);
        }

        bool setAt(const T& value, int i)
        {
            return m_q->setAtUnsafe(value, i);
        }

        bool removeAt(int index)
        {
            return m_q->removeAtUnsafe(index);
        }

        void pack()
        {
            m_q->packUnsafe();
        }

        bool popFront()
        {
            if (m_q->isEmpty())
                return false;

            m_q->m_buffer[m_q->m_headIndex++] = T();
            if (m_q->m_headIndex >= (int) m_q->m_buffer.size())
                m_q->m_headIndex = 0;
            --m_q->m_bufferLen;
            return true;
        }

        const T& front() const
        {
            return at(0);
        }

        const T& last() const
        {
            return at(size() - 1);
        }

        int size() const
        {
            return m_q->size();
        }

        void clear()
        {
            m_q->clearUnsafe();
        }

    private:
        explicit RandomAccess(Q* queue): m_q(queue)
        {
            m_q->lockInternal();
        }

    private:
        Q* m_q = nullptr;

        friend class QnSafeQueue<T>;
    };

    /**
     * @param maxSize Is not used by the implementation of this class, but rather intended to be
     *     checked by pushers via maxSize().
     */
    explicit QnSafeQueue(quint32 maxSize = kDefaultMaxThreadQueueSize) : m_maxSize(maxSize)
    {
        reallocateBufferUnsafe(maxSize);
    }

    RandomAccess<const QnSafeQueue> lock() const
    {
        return RandomAccess<const QnSafeQueue>(const_cast<QnSafeQueue*>(this));
    }

    RandomAccess<QnSafeQueue> lock()
    {
        return RandomAccess<QnSafeQueue>(this);
    }

    bool isEmpty() const
    {
        return size() == 0;
    }

    template<typename ValueRef>
    bool push(ValueRef&& val)
    {
        NX_MUTEX_LOCKER mutex(&m_mutex);
        if (m_terminated)
            return false;

        if ((uint)m_bufferLen == m_buffer.size())
            reallocateBufferUnsafe(qMax(m_bufferLen + 1, m_bufferLen + m_bufferLen/4));

        const int index = (m_headIndex + m_bufferLen) % m_buffer.size();
        m_buffer[index] = std::forward<ValueRef>(val);
        m_bufferLen++;

        m_waitCond.wakeOne();

        return true;
    }

    bool pop(T& val, std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
    {
        NX_MUTEX_LOCKER mutex(&m_mutex);
        if (!m_terminated && isEmpty())
            m_waitCond.wait(&m_mutex, timeout);

        if (m_terminated || isEmpty())
            return false;

        val = std::exchange(m_buffer[m_headIndex], T());
        m_headIndex++;
        if ((uint)m_headIndex >= m_buffer.size())
            m_headIndex = 0;
        m_bufferLen--;
        return true;
    }

    template <class ConditionFunc>
    void detachDataByCondition(const ConditionFunc& cond, const QVariant& opaque = QVariant())
    {
        NX_MUTEX_LOCKER mutex(&m_mutex);
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
        NX_MUTEX_LOCKER mutex(&m_mutex);
        m_maxSize = value;
        reallocateBufferUnsafe(std::max((int) m_maxSize, (int) m_buffer.size()));
    }

    void clear()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        clearUnsafe();
        m_waitCond.wakeOne();
    }

    /**
     * Queue in terminated mode will not wait for new data if empty.
     * But pop call returns already queued data anyway.
     */
    void setTerminated(bool value)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
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

    bool isValidIndex(int index) const
    {
        return index >= 0 && index < size();
    }

    const T& atUnsafe(int i) const
    {
        static const T empty{};
        if (!isValidIndex(i))
            return empty;

        const int index = m_headIndex + i;
        return m_buffer[index % m_buffer.size()];
    }

    bool setAtUnsafe(const T& value, int i)
    {
        if (!isValidIndex(i))
            return false;

        const int index = m_headIndex + i;
        m_buffer[index % m_buffer.size()] = value;
        return true;
    }

    bool removeAtUnsafe(int index)
    {
        if (!isValidIndex(index))
            return false;

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
        return true;
    }

    void packUnsafe()
    {
        int emptyElementIndex = -1;
        int emptyElements = 0;
        for (int index = 0; index < m_bufferLen; ++index)
        {
            const int bufferIndex = (m_headIndex + index) % m_buffer.size();
            if (m_buffer[bufferIndex] == T())
            {
                if (emptyElementIndex == -1)
                    emptyElementIndex = bufferIndex;
                ++emptyElements;
            }
            else if (emptyElementIndex != -1)
            {
                std::swap(m_buffer[emptyElementIndex], (m_buffer[bufferIndex]));
                emptyElementIndex = (emptyElementIndex + 1) % m_buffer.size();
            }
        }
        m_bufferLen -= emptyElements;
    }

    // For grow only.
    void reallocateBufferUnsafe(int newSize)
    {
        const int oldSize = (int) m_buffer.size();
        m_buffer.resize(newSize);

        if (m_headIndex > 0 && m_bufferLen > 0 && newSize > oldSize)
        {
            int tailIndex = m_headIndex + m_bufferLen;
            if (tailIndex <= oldSize)
                return; //< No correction is needed.

            tailIndex -= oldSize;
            const int delta = newSize - oldSize;

            for (int i = 0; i < delta && i < tailIndex; ++i)
                m_buffer[oldSize + i] = std::move(m_buffer[i]);
            int i = 0;
            for (;i < tailIndex - delta; ++i)
                m_buffer[i] = std::move(m_buffer[i + delta]);
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
    int m_headIndex = 0;
    std::atomic<int> m_bufferLen = 0;
    std::atomic<int> m_maxSize = 0;

    mutable nx::Mutex m_mutex;
    mutable nx::WaitCondition m_waitCond;
    bool m_terminated = false;
};

template <typename T>
class QnUnsafeQueue
{
public:
    QnUnsafeQueue( quint32 maxSize = kDefaultMaxThreadQueueSize)
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

        // we can have 2 threads independently put data at the same queue; so we need to put data any way. client is responsible for max size of the quue
        //if ( m_queue.size()>=m_maxSize )    return false; <- wrong approach

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
