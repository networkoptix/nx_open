#ifndef OBJECT_COUNTER
#define OBJECT_COUNTER

#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include <queue>
#include <sstream>

#if 0
#  include <time.h>
#  define debug_printf(...) fprintf(stderr, "<%.1f> ", clock()/100000.0), fprintf(stderr, __VA_ARGS__)
#else
#  define debug_printf(...)
#endif
class Logger
{
    typedef std::queue<std::string> TaskQueue;
public:
    Logger()
        : m_needStop(false),
          m_startPoint(std::chrono::system_clock::now())
    {
        m_thread = std::thread(
            [this]
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lk(m_mutex);
                    m_cond.wait(lk, [this]{ return !m_queue.empty() || m_needStop; });

                    while (!m_queue.empty())
                    {
                        auto logMessage = m_queue.front();
                        m_queue.pop();
                        std::cout << logMessage << std::endl;
                    }

                    if (m_needStop)
                        return;
                }
            });
    }

    ~Logger()
    {
        m_needStop = true;
        m_cond.notify_all();
        m_thread.join();
    }

    Logger & operator << (const std::string &s)
    {
        if (m_needStop)
            return *this;

        std::stringstream ss;
        auto timeNow = std::chrono::system_clock::now();
        ss << "(" << std::hex << std::this_thread::get_id() << ") ["
           << (std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - m_startPoint).count() / 1000.0)
           << "] ";

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_queue.push(ss.str() + s);
        }
        m_cond.notify_all();
        return *this;
    }

private:
    TaskQueue m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::thread m_thread;
    std::atomic<bool> m_needStop;
    std::chrono::time_point<std::chrono::system_clock> m_startPoint;
};

extern Logger logger;

#define NO_LOG 1
#define ITE_LOG() if (NO_LOG) {} else logger

#define FMT(...) [&] \
    { \
        char buf[1024]; \
        sprintf(buf, __VA_ARGS__); \
        return std::string(buf); \
    }()

#ifdef COUNT_OBJECTS

#include <atomic>

#define INIT_OBJECT_COUNTER(T) \
    template <> std::atomic_uint ObjectCounter<T>::ctor_(0); \
    template <> std::atomic_uint ObjectCounter<T>::dtor_(0); \
    template <> const char * ObjectCounter<T>::name_ = #T;

namespace ite
{
    template <typename T>
    class ObjectCounter
    {
    public:
        ObjectCounter<T>()
        {
            ++ctor_;
        }

        ~ObjectCounter<T>()
        {
            ++dtor_;
        }

        static const char * name() { return name_; }
        static unsigned ctorCount() { return ctor_; }
        static unsigned dtorCount() { return dtor_; }
        static unsigned diffCount() { return ctor_ - dtor_; }

    private:
        static const char * name_;
        static std::atomic_uint ctor_;
        static std::atomic_uint dtor_;
    };
}

#else

namespace ite
{
    template<typename T>
    class ObjectCounter
    {};
}

#endif

#endif
