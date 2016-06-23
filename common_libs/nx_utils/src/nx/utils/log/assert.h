#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

#include "log_message.h"

// Uncomment enable NX_CHECK condition time measurements
//#define NX_CHECK_MEASURE_TIME


namespace nx {
namespace utils {

NX_UTILS_API void logError(const lm& message);

template<typename Reason>
lm assertLog(const char* file, int line, const char* condition, const Reason& message)
{
    const auto out = lm("FAILURE %1:%2 NX_CHECK(%3) %4")
        .arg(file).arg(line).arg(condition).arg(message);

    logError(out);
    return out;
}

template<typename ... Arguments>
void assertCrash(Arguments&& ... args)
{
    const auto out  = assertLog(std::forward<Arguments>(args)...);
    std::cerr << std::endl << ">>>" << out.toStdString() << std::endl;
    *reinterpret_cast<volatile int*>(0) = 7;
}

class NX_UTILS_API AssertTimer
{
public:
    struct NX_UTILS_API TimeInfo
    {
        TimeInfo();
        TimeInfo(const TimeInfo& info);
        TimeInfo& operator =(const TimeInfo& info);

        bool operator <(const TimeInfo& other) const;
        void add(std::chrono::microseconds duration);

        size_t count() const;
        std::chrono::microseconds time() const;

    private:
        std::atomic<size_t> m_count;
        std::atomic<size_t> m_time;
    };

    TimeInfo* info(const char* file, int line);
    ~AssertTimer();

    #ifdef NX_CHECK_MEASURE_TIME
        static AssertTimer instance;
    #endif

private:
    std::mutex m_mutex;
    std::map<std::string, TimeInfo> m_times;
};

} // namespace utils
} // namespace nx

#ifdef NX_CHECK_MEASURE_TIME
    #define NX_CHECK(condition, message, action) \
        do { \
            auto begin = std::chrono::system_clock::now(); \
            auto isOk = static_cast<bool>(condition); \
            auto time = std::chrono::system_clock::now() - begin; \
            \
            static const auto info = nx::utils::AssertTimer::instance.info(__FILE__, __LINE__); \
            info->add(std::chrono::duration_cast<std::chrono::microseconds>(time)); \
            \
            if (!isOk) \
                nx::utils::assert##action(__FILE__, __LINE__, #condition, message); \
        } while (false)
#else
    #define NX_CHECK(condition, message, action) \
        do { \
            if (!(condition)) \
                nx::utils::assert##action(__FILE__, __LINE__, #condition, message); \
        } while (false)
#endif

#define NX_CRITICAL_IMPL(condition, message) NX_CHECK(condition, message, Crash)

#ifdef _DEBUG
    #define NX_ASSERT_IMPL(condition, message) NX_CHECK(condition, message, Crash)
    #define NX_EXPECT_IMPL(condition, message) NX_CHECK(condition, message, Crash)
#else
    #define NX_ASSERT_IMPL(condition, message) NX_CHECK(condition, message, Log)
    #define NX_EXPECT_IMPL(condition, message)
#endif

#define NX_MSVC_EXPAND(x) x
#define NX_GET_4TH_ARG(a1, a2, a3, a4, ...) a4


#define NX_CRITICAL1(condition) \
    NX_CRITICAL_IMPL(condition, "")

#define NX_CRITICAL2(condition, message) \
    NX_CRITICAL_IMPL(condition, message)

#define NX_CRITICAL3(condition, where, message) \
    NX_CRITICAL_IMPL(condition, lm("[%1] %2").arg(where).arg(message))

/** debug & release: Leads to segfault in case of failure */
#define NX_CRITICAL(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_CRITICAL3, NX_CRITICAL2, NX_CRITICAL1, args_reqired)(__VA_ARGS__))


#define NX_ASSERT1(condition) \
    NX_ASSERT_IMPL(condition, "")

#define NX_ASSERT2(condition, message) \
    NX_ASSERT_IMPL(condition, message)

#define NX_ASSERT3(condition, where, message) \
    NX_ASSERT_IMPL(condition, lm("[%1] %2").arg(where).arg(message))

/** debug: Leads to segfault in case of failure
 *  release: Writes NX_LOG in case of failure */
#define NX_ASSERT(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_ASSERT3, NX_ASSERT2, NX_ASSERT1, args_reqired)(__VA_ARGS__))


#define NX_EXPECT1(condition) \
    NX_EXPECT_IMPL(condition, "")

#define NX_EXPECT2(condition, message) \
    NX_EXPECT_IMPL(condition, message)

#define NX_EXPECT3(condition, where, message) \
    NX_EXPECT_IMPL(condition, lm("[%1] %2").arg(where).arg(message))

/** debug: Leads to segfault in case of failure
 *  release: Does nothing (condition is not even evaluated) */
#define NX_EXPECT(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_EXPECT3, NX_EXPECT2, NX_EXPECT1, args_reqired)(__VA_ARGS__))
