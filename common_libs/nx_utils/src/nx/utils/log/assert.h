#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <functional>

#include "log_message.h"

#if defined(ANDROID) || defined(__ANDROID__)
    #include "backtrace_android.h"
#endif // defined(ANDROID) || defined(__ANDROID__)

// Uncomment to enable NX_CHECK condition time measurements:
//#define NX_CHECK_MEASURE_TIME

namespace nx {
namespace utils {

NX_UTILS_API void logAssert(const log::Message& message);
void NX_UTILS_API setOnAssertHandler(std::function<void(const log::Message&)> handler);

template<typename Reason>
log::Message assertLog(const char* file, int line, const char* condition, const Reason& message)
{
   #if defined(ANDROID) || defined(__ANDROID__)
       const auto out = lm("ASSERTION FAILED: %1:%2 (%3) %4\nAndroid backtrace:\n%5")
           .arg(file).arg(line).arg(condition).arg(message).arg(buildBacktrace());
   #else
        const auto out = lm("ASSERTION FAILED: %1:%2 (%3) %4")
            .arg(file).arg(line).arg(condition).arg(message);
   #endif

    logAssert(out);
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
        TimeInfo& operator=(const TimeInfo& info);

        bool operator<(const TimeInfo& other) const;
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
    #define NX_CHECK(CONDITION, MESSAGE, ACTION) do \
    { \
        auto begin = std::chrono::steady_clock::now(); \
        bool isOk = static_cast<bool>(condition); \
        auto time = std::chrono::steady_clock::now() - begin; \
        \
        static const auto info = nx::utils::AssertTimer::instance.info(__FILE__, __LINE__); \
        info->add(std::chrono::duration_cast<std::chrono::microseconds>(time)); \
        \
        if (!isOk) \
            nx::utils::assert##ACTION(__FILE__, __LINE__, #CONDITION, MESSAGE); \
    } while (0)
#else
    #define NX_CHECK(CONDITION, MESSAGE, ACTION) do \
    { \
        if (!(CONDITION)) \
            nx::utils::assert##ACTION(__FILE__, __LINE__, #CONDITION, MESSAGE); \
    } while (0)
#endif

#define NX_CRITICAL_IMPL(CONDITION, MESSAGE) NX_CHECK(CONDITION, MESSAGE, Crash)

#ifdef _DEBUG
    #define NX_ASSERT_IMPL(CONDITION, MESSAGE) NX_CHECK(CONDITION, MESSAGE, Crash)
    #define NX_EXPECT_IMPL(CONDITION, MESSAGE) NX_CHECK(CONDITION, MESSAGE, Crash)
#else
    #define NX_ASSERT_IMPL(CONDITION, MESSAGE) NX_CHECK(CONDITION, MESSAGE, Log)
    #define NX_EXPECT_IMPL(CONDITION, MESSAGE) /* DOES NOTHING */
#endif

/**
 * Needed as a workaround for an MSVC issue: if __VA_ARGS__ is used as an argument to another
 * macro, it forms a single macro argument even if contains commas.
 */
#define NX_MSVC_EXPAND(x) x

/** Useful to convert a single macro arg which has form (A, B, ...) to arg list: A, B, ... */
#define NX_REMOVE_PARENTHESES(...) __VA_ARGS__

#define NX_GET_2ND_ARG(a1, a2, ...) a2
#define NX_GET_3RD_ARG(a1, a2, a3, ...) a3
#define NX_GET_4TH_ARG(a1, a2, a3, a4, ...) a4

#define NX_CRITICAL1(CONDITION) \
    NX_CRITICAL_IMPL(CONDITION, "")

#define NX_CRITICAL2(CONDITION, MESSAGE) \
    NX_CRITICAL_IMPL(CONDITION, MESSAGE)

#define NX_CRITICAL3(CONDITION, WHERE, MESSAGE) \
    NX_CRITICAL_IMPL(CONDITION, lm("[%1] %2").arg(WHERE).arg(MESSAGE))

/**
 * Debug and Release: Cause segfault in case of failure.
 */
#define NX_CRITICAL(...) \
    NX_MSVC_EXPAND(NX_GET_4TH_ARG( \
        __VA_ARGS__, NX_CRITICAL3, NX_CRITICAL2, NX_CRITICAL1, args_reqired)(__VA_ARGS__))

#define NX_ASSERT1(CONDITION) \
    NX_ASSERT_IMPL(CONDITION, "")

#define NX_ASSERT2(CONDITION, MESSAGE) \
    NX_ASSERT_IMPL(CONDITION, MESSAGE)

#define NX_ASSERT3(CONDITION, WHERE, MESSAGE) \
    NX_ASSERT_IMPL(CONDITION, lm("[%1] %2").arg(WHERE).arg(MESSAGE))

/**
 * Debug: Cause segfault in case of failure.
 * Release: Write NX_LOG in case of failure.
 */
#define NX_ASSERT(...) \
    NX_MSVC_EXPAND(NX_GET_4TH_ARG( \
        __VA_ARGS__, NX_ASSERT3, NX_ASSERT2, NX_ASSERT1, args_reqired)(__VA_ARGS__))

#define NX_EXPECT1(CONDITION) \
    NX_EXPECT_IMPL(CONDITION, "")

#define NX_EXPECT2(CONDITION, MESSAGE) \
    NX_EXPECT_IMPL(CONDITION, MESSAGE)

#define NX_EXPECT3(CONDITION, WHERE, MESSAGE) \
    NX_EXPECT_IMPL(CONDITION, lm("[%1] %2").arg(WHERE).arg(MESSAGE))

/**
 * Debug: Leads to segfault in case of failure.
 * Release: Does nothing (condition is not even evaluated).
 */
#define NX_EXPECT(...) \
    NX_MSVC_EXPAND(NX_GET_4TH_ARG( \
        __VA_ARGS__, NX_EXPECT3, NX_EXPECT2, NX_EXPECT1, args_reqired)(__VA_ARGS__))
