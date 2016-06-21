#pragma once

#include <iostream>
#include <sstream>

#include "log_message.h"

// Uncomment to assert condition enable time measurements
#define NX_ASSERT_MEASURE_TIME

#ifdef NX_ASSERT_MEASURE_TIME
    #include <string>
    #include <mutex>
    #include <map>
    #include <chrono>
#endif

namespace nx {
namespace utils {

NX_UTILS_API void logError(const lm& message);

template<typename Reason>
lm assertLog(const char* file, int line, const char* condition, const Reason& message)
{
    const auto out = lm("FAILURE %1:%2 NX_ASSERT(%3) %4")
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

#ifdef NX_ASSERT_MEASURE_TIME
    class NX_UTILS_API AssertTimer
    {
    public:
        void add(const char* file, int line, std::chrono::microseconds time);
        ~AssertTimer();

        static AssertTimer instance;

    private:
        std::mutex m_mutex;
        std::map<std::string, std::pair<size_t, std::chrono::microseconds>> m_times;
    };
#endif

} // namespace utils
} // namespace nx

#ifndef NX_ASSERT_MEASURE_TIME
    // Causes segfault in debug and release
    #define NX_CRITICAL_IMPL(condition, message) \
        if (!(condition)) \
            nx::utils::assertCrash(__FILE__, __LINE__, #condition, message)
#else
    template<typename ConditionFunction, typename Reason>
    void nxCriticalImpl(
        const ConditionFunction& conditionFunction,
        const char* file, int line, const char* condition, const Reason& message)
    {
        auto begin = std::chrono::system_clock::now();
        bool isOk = conditionFunction();
        auto total = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - begin);

        nx::utils::AssertTimer::instance.add(file, line, total);
        if (!isOk)
            nx::utils::assertCrash(file, line, condition, message);
    }

    #define NX_CRITICAL_IMPL(condition, message) \
        nxCriticalImpl( \
            [&](){ return static_cast<bool>(condition); }, \
            __FILE__, __LINE__, #condition, message)
#endif

#ifdef _DEBUG
    // Every expectation is critical in debug
    #define NX_ASSERT_IMPL(condition, message) NX_CRITICAL_IMPL(condition, message)
    #define NX_EXPECT_IMPL(condition, message) NX_CRITICAL_IMPL(condition, message)
#else
    #define NX_ASSERT_IMPL(condition, message) \
        if (!(condition)) \
            nx::utils::assertLog(__FILE__, __LINE__, #condition, message)

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
 *  release: Does nothing (condition is not even evaluates) */
#define NX_EXPECT(...) NX_MSVC_EXPAND( \
    NX_GET_4TH_ARG(__VA_ARGS__, NX_EXPECT3, NX_EXPECT2, NX_EXPECT1, args_reqired)(__VA_ARGS__))
