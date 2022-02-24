// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <functional>

#include "format.h"

#include <nx/utils/general_macros.h>
#include <nx/utils/nx_utils_ini.h>

// Uncomment to enable NX_CHECK condition time measurements:
//#define NX_CHECK_MEASURE_TIME

namespace nx::utils {

/**
 * @param value if true, stack trace is logged on assertion failure and printed to stderr.
 *     Default value is false.
 */
void NX_UTILS_API setPrintStackTraceOnAssertEnabled(bool value);
void NX_UTILS_API setOnAssertHandler(std::function<void(const QString&)> handler);
void NX_UTILS_API crashProgram(const QString& message);

/**
 * @return Always false.
 */
bool NX_UTILS_API assertFailure(bool isCritical, const QString& message);

void NX_UTILS_API enableQtMessageAsserts();
void NX_UTILS_API disableQtMessageAsserts();

namespace detail {

static const bool assertHeavyConditionEnabled = ini().assertHeavyCondition;

} // detail

/**
 * @return Always false.
 */
template<typename... Args>
bool assertFailure(
    bool isCritical, const char* file, int line, const char* condition, Args... args)
{
    // NOTE: If message is empty, an extra space will appear before newline, which is hard to avoid.
    const auto out = NX_FMT("ASSERTION FAILED: %1:%2 (%3) %4",
        file, line, condition, nx::format(std::forward<Args>(args)...));
    return assertFailure(isCritical, out);
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

} // namespace nx::utils

#if defined(NX_CHECK_MEASURE_TIME)
    #define NX_CHECK(IS_CRITICAL, CONDITION, MESSAGE) ( \
        [begin = std::chrono::steady_clock::now(), \
            result = (CONDITION) || ::nx::utils::assertFailure( \
                IS_CRITICAL, __FILE__, __LINE__, #CONDITION, MESSAGE)]() \
        { \
            const auto time = std::chrono::steady_clock::now() - begin; \
            static const auto info = nx::utils::AssertTimer::instance.info(__FILE__, __LINE__); \
            info->add(std::chrono::duration_cast<std::chrono::microseconds>(time)); \
            return result; \
        }() \
    )
#else
    #define NX_CHECK(IS_CRITICAL, CONDITION, MESSAGE) ( \
        [result = (CONDITION) || ::nx::utils::assertFailure( \
            IS_CRITICAL, __FILE__, __LINE__, #CONDITION, MESSAGE)]() \
        { \
            return result; \
        }() \
    )
#endif

/**
 * - Debug and Release: Causes segfault in case of failure.
 * - Usage: Unrecoverable situations when later crash or deadlock is inevitable.
 *     ```
 *         NX_CRITICAL(veryImportantObjectPointer);
 *     ```
 */
#define NX_CRITICAL(CONDITION, ...) \
    NX_CHECK(/*isCritical*/ true, CONDITION, ::nx::format(__VA_ARGS__))

/**
 * - Debug: Causes segfault in case of assertion failure, if not disabled via
 *     nx_utils.ini assertCrash = 0.
 * - Release: Writes log in case of assertion failure, if not enabled via
 *     nx_utils.ini assertCrash = 0.
 * - Usage: Recoverable situations, application must keep going after such failure.
 *     ```
 *         if (!NX_ASSERT(objectPointer))
 *             return false;
 *
 *         NX_ASSERT(!objectPointer->name().isEmpty());
 *     ```
 * @return Condition evaluation result.
 */
#define NX_ASSERT(CONDITION, ...) \
    NX_CHECK(/*isCritical*/ false, CONDITION, ::nx::format(__VA_ARGS__))

/**
 * - Debug: Works the same way as NX_ASSERT(), if not disabled via
 *     nx_utils.ini assertHeavyCondition = 0.
 * - Release: Does nothing (the condition is never evaluated).
 *
 * - Usage: The same as NX_ASSERT() but for cases when condition evaluation may affect application
 *     performance.
 *     ```
 *         NX_ASSERT_HEAVY_CONDITION(!getObjectFromDb().isEmpty());
 *     ```
 */
#define NX_ASSERT_HEAVY_CONDITION(CONDITION, ...) do \
{ \
    if (::nx::utils::detail::assertHeavyConditionEnabled) \
        NX_CHECK(/*isCritical*/ false, CONDITION, ::nx::format(__VA_ARGS__)); \
} while (0)
