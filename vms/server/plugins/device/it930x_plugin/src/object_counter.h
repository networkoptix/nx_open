#ifndef OBJECT_COUNTER
#define OBJECT_COUNTER

#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include <queue>
#include <sstream>
#include <fstream>

#include <nx/utils/std/thread.h>

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
    Logger(const std::string &fname = "")
        : m_needStop(false),
          m_startPoint(std::chrono::system_clock::now())
    {
//        std::cout << "[LOGGER] Starting..." << std::endl;
//        std::cout << "[LOGGER] Using " << (fname.empty() ? "stdout" : fname.c_str()) << " as log sink" << std::endl;
        if (fname.empty())
            m_ostream = std::shared_ptr<std::ostream>(&std::cout, [] (std::ostream *) {});
        else
        {
            m_ostream = std::shared_ptr<std::ostream>(new std::ofstream(fname));
            if (!*m_ostream)
            {
                //std::cout << "[It930x LOGGER] Log file open failed. Logging to stdout" << std::endl;
                m_ostream.reset();
                m_ostream = std::shared_ptr<std::ostream>(&std::cout, [] (std::ostream *) {});
            }
        }

        m_thread = nx::utils::thread(
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
                        if (m_ostream)
                            *m_ostream << logMessage << std::endl;
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
    nx::utils::thread m_thread;
    std::atomic<bool> m_needStop;
    std::chrono::time_point<std::chrono::system_clock> m_startPoint;
    std::shared_ptr<std::ostream> m_ostream;
};

extern Logger logger;

#if 0
#   define NO_LOG 0
#else
#   define NO_LOG 1
#endif

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
