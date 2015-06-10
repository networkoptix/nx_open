#include <mutex>
#include <condition_variable>
#include <chrono>

namespace rw_lock
{
    //
    class OneWriterManyReaders
    {
    public:
        struct Scope
        {
            OneWriterManyReaders& m_rw;

            Scope(OneWriterManyReaders& rw)
            :   m_rw(rw)
            {}

            ~Scope()
            {
                m_rw.unlock();
            }
        };

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

        void writeLock(unsigned timeoutMS = 10)
        {
            std::chrono::milliseconds timeout(timeoutMS);

            for (;;)
            {
                std::unique_lock<std::mutex> lock(m_mutex); // LOCK

                if (m_readers == 0)
                {
                    m_readers = -1;
                    break;
                }

                m_cv.wait_for(lock, timeout); // WAIT. Could miss notify event - timeout
            }
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
}
