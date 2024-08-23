// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <condition_variable>
#include <mutex>

namespace rw_lock
{
    ///
    class OneWriterManyReaders
    {
    public:
        OneWriterManyReaders()
        :   m_readers(0)
        {}

        bool tryReadLock()
        {
            std::unique_lock<std::mutex> lock(m_mutex); // LOCK

            if (m_readers >= 0)
            {
                ++m_readers;
                return true;
            }
            return false;
        }

        bool tryWriteLock()
        {
            std::unique_lock<std::mutex> lock(m_mutex); // LOCK

            if (m_readers == 0)
            {
                m_readers = -1;
                return true;
            }
            return false;
        }

        void writeLock()
        {
            std::unique_lock<std::mutex> lock(m_mutex); // LOCK

            if (m_readers != 0)
                m_cv.wait(lock, [=]{ return m_readers == 0; } );

            m_readers = -1;
        }

        void unlock()
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex); // LOCK

                if (m_readers > 0)
                    --m_readers;
                else if (m_readers < 0)
                    ++m_readers;
            }

            m_cv.notify_one(); // NOTIFY
        }

    private:
        std::mutex m_mutex;
        int32_t m_readers;
        std::condition_variable m_cv;
    };

    typedef enum
    {
        READ,
        WRITE,
        TRY_READ,
        TRY_WRITE
    } LockType;

    ///
    template<LockType LT>
    class ScopedLock
    {
    public:
        ScopedLock(OneWriterManyReaders& rw)
        :   m_rw(rw),
            m_own(false)
        {
            acquire();
        }

        ~ScopedLock()
        {
            if (m_own)
                m_rw.unlock();
        }

        operator bool () const { return m_own; }

    private:
        OneWriterManyReaders& m_rw;
        bool m_own;

        void acquire();
    };

    template <> inline void ScopedLock<WRITE>::acquire()
    {
        m_rw.writeLock();
        m_own = true;
    }

    template <> inline void ScopedLock<TRY_READ>::acquire()
    {
        m_own = m_rw.tryReadLock();
    }
}
