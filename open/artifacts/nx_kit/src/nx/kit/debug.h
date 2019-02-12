#pragma once

/**@file
 * Utilities for debugging: measuring execution time and FPS, working with strings, logging values
 * and messages.
 *
 * This unit can be compiled in the context of any C++ project.
 */

#include <iostream>
#include <cstdint>
#include <functional>
#include <sstream>
#include <memory>

#include <nx/kit/utils.h>

#if !defined(NX_KIT_API)
    #define NX_KIT_API /*empty*/
#endif

namespace nx {
namespace kit {
namespace debug {

//-------------------------------------------------------------------------------------------------
// Tools

/** @return Separator in __FILE__ - slash or backslash, or '\0' if unknown. Cached in static. */
NX_KIT_API char pathSeparator();

NX_KIT_API size_t commonPrefixSize(const std::string& s1, const std::string& s2);

/**
 * Extracts part of the source code filename which follows the uninteresting common part.
 * @return Pointer to some char inside the given string.
 */
NX_KIT_API const char* relativeSrcFilename(const char* file);

NX_KIT_API std::string fileBaseNameWithoutExt(const char* file);

//-------------------------------------------------------------------------------------------------
// Output

/** Define if needed to redirect NX_PRINT... to qDebug(). */
#if defined(NX_PRINT_TO_QDEBUG)
    #define NX_DEBUG_STREAM qDebug().nospace().noquote()
    #define NX_DEBUG_ENDL /*empty*/
    static inline QDebug operator<<(QDebug d, const std::string& s)
    {
        return d << QString::fromStdString(s);
    }
#endif

#if !defined(NX_DEBUG_INI)
    /** If needed, redefine to form a prefix for NX_DEBUG_ENABLE... macros. */
    #define NX_DEBUG_INI ini().
#endif

#if !defined(NX_DEBUG_ENABLE_OUTPUT)
    /** Redefine if needed. */
    #define NX_DEBUG_ENABLE_OUTPUT NX_DEBUG_INI enableOutput
#endif

#if !defined(NX_PRINT_PREFIX)
    /** Redefine if needed; used as the first item in NX_PRINT and NX_OUTPUT. */
    #define NX_PRINT_PREFIX ::nx::kit::debug::detail::printPrefix(__FILE__)
#endif

#if !defined(NX_DEBUG_STREAM)
    /** Redefine if needed; used for all output by other macros. */
    #define NX_DEBUG_STREAM *::nx::kit::debug::stream()
#endif

#if !defined(NX_DEBUG_ENDL)
    /** Redefine if needed; used after NX_DEBUG_STREAM by other macros. */
    #define NX_DEBUG_ENDL << std::endl
#endif

/**
 * Allows to override the stream used by NX_PRINT via NX_DEBUG_STREAM, initially std::cerr.
 * @return Reference to the static variable which stores the stream pointer.
 */
NX_KIT_API std::ostream*& stream();

#if !defined(NX_PRINT)
    /**
     * Print the args to NX_DEBUG_STREAM, starting with NX_PRINT_PREFIX and ending with
     * NX_DEBUG_ENDL. Redefine if needed.
     */
    #define NX_PRINT /* << args... */ \
        /* Allocate a temp value, which prints endl in its destructor, in the "<<" expression. */ \
        ( []() { struct Endl { ~Endl() { NX_DEBUG_STREAM NX_DEBUG_ENDL; } }; \
            return std::make_shared<Endl>(); }() ) /*operator,*/, \
        NX_DEBUG_STREAM << NX_PRINT_PREFIX
#endif

/**
 * Prints the args like NX_PRINT; does nothing if !NX_DEBUG_ENABLE_OUTPUT.
 */
#define NX_OUTPUT /* << args... */ \
    for (/* Executed either once or never; `for` instead of `if` gives no warnings. */ \
        int NX_KIT_DEBUG_DETAIL_CONCAT(nxOutput_, __line__) = 0; \
        NX_KIT_DEBUG_DETAIL_CONCAT(nxOutput_, __line__) != 1 && (NX_DEBUG_ENABLE_OUTPUT); \
        ++NX_KIT_DEBUG_DETAIL_CONCAT(nxOutput_, __line__) \
    ) NX_PRINT

//-------------------------------------------------------------------------------------------------
// Assertions

/**
 * If the condition is false, logs the failure with NX_PRINT, and in debug build (i.e. NDEBUG is
 * not defined), crashes the process to let a dump/core be generated.
 *
 * ATTENTION: Unlike std library assert(), the condition is checked even in Release build to log
 * the failure.
 *
 * Additionally, allows to handle the failure (because the application must keep going):
 * ```
 *     if (!NX_KIT_ASSERT(objectPointer))
 *         return false;
 * ```
 *
 * @return Condition evaluation result.
 */
#define NX_KIT_ASSERT(/* CONDITION, MESSAGE = "" */ ...) \
    NX_KIT_DEBUG_DETAIL_MSVC_EXPAND(NX_KIT_DEBUG_DETAIL_GET_3RD_ARG( \
        __VA_ARGS__, NX_KIT_DEBUG_DETAIL_ASSERT2, NX_KIT_DEBUG_DETAIL_ASSERT1, \
        /* Helps to generate a reasonable compiler error. */ args_required)(__VA_ARGS__))

//-------------------------------------------------------------------------------------------------
// Print info

/**
 * Logs execution of a line - to use, write LL at the beginning of the line.
 */
#define LL \
    NX_PRINT << "####### LL line " << __LINE__ \
        << NX_KIT_DEBUG_DETAIL_THREAD_ID() \
        << ", file " << ::nx::kit::debug::relativeSrcFilename(__FILE__);

/**
 * Prints the expression text and its value via toString().
 */
#define NX_PRINT_VALUE(VALUE) \
    NX_PRINT << "####### " #VALUE ": " << ::nx::kit::utils::toString((VALUE))

/**
 * Hex-dumps binary data using NX_PRINT.
 */
#define NX_PRINT_HEX_DUMP(CAPTION, BYTES, SIZE) \
    ::nx::kit::debug::detail::printHexDump( \
        NX_KIT_DEBUG_DETAIL_PRINT_FUNC, (CAPTION), (const char*) (BYTES), (int) (SIZE))

//-------------------------------------------------------------------------------------------------
// Time

#if !defined(NX_DEBUG_ENABLE_TIME)
    /** Redefine if needed. */
    #define NX_DEBUG_ENABLE_TIME NX_DEBUG_INI enableTime
#endif

/**
 * Starts measuring time - saves current time in a variable; does nothing if !NX_DEBUG_ENABLE_TIME.
 */
#define NX_TIME_BEGIN(TAG) \
    ::nx::kit::debug::detail::Timer nxTimer_##TAG( \
        (NX_DEBUG_ENABLE_TIME), NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #TAG)

/**
 * Measures the time in the intermediate point, to be printed at the end; does nothing if
 * !NX_DEBUG_ENABLE_TIME. If called at least once, forces "ms" instead of "us" for the totals.
 */
#define NX_TIME_MARK(TAG, MARK) do \
{ \
    if (NX_DEBUG_ENABLE_TIME) \
        nxTimer_##TAG.mark((MARK)); \
} while (0)

/**
 * Prints the totals of this time measurement with NX_PRINT; does nothing if !NX_DEBUG_ENABLE_TIME.
 */
#define NX_TIME_END(TAG) do \
{ \
    if (NX_DEBUG_ENABLE_TIME) \
        nxTimer_##TAG.finish(); \
} while (0)

//-------------------------------------------------------------------------------------------------
// Fps

#if !defined(NX_DEBUG_ENABLE_FPS)
    /** Redefine if needed. Will be suffixed with Fps TAG values. */
    #define NX_DEBUG_ENABLE_FPS NX_DEBUG_INI enableFps
#endif

/**
 * Measures and prints with NX_PRINT each period between executions of the point of call; does
 * nothing if !NX_DEBUG_ENABLE_FPS##TAG.
 */
#define NX_FPS(TAG, /*OPTIONAL_MARK*/...) do \
{ \
    if (NX_KIT_DEBUG_DETAIL_CONCAT(NX_DEBUG_ENABLE_FPS, TAG)) \
    { \
        static ::nx::kit::debug::detail::Fps fps( \
            NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #TAG); \
        fps.mark(__VA_ARGS__); \
    } \
} while (0)

//-------------------------------------------------------------------------------------------------
// Implementation

namespace detail {

typedef std::function<void(const char*)> PrintFunc;

#define NX_KIT_DEBUG_DETAIL_PRINT_FUNC ([&](const char* message) { NX_PRINT << message; })

/**
 * Needed as a workaround for an MSVC issue: if __VA_ARGS__ is used as an argument to another
 * macro, it forms a single macro argument even if contains commas.
 */
#define NX_KIT_DEBUG_DETAIL_MSVC_EXPAND(ARG) ARG

/** Needed to make an "overloaded" macro which can accept either one or two args. */
#define NX_KIT_DEBUG_DETAIL_GET_3RD_ARG(ARG1, ARG2, ARG3, ...) ARG3

#define NX_KIT_DEBUG_DETAIL_ASSERT1(CONDITION) \
    ::nx::kit::debug::detail::doAssert( \
        CONDITION, NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #CONDITION, "", __FILE__, __LINE__)

#define NX_KIT_DEBUG_DETAIL_ASSERT2(CONDITION, MESSAGE) \
    ::nx::kit::debug::detail::doAssert( \
        CONDITION, NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #CONDITION, MESSAGE, __FILE__, __LINE__)

NX_KIT_API void assertionFailed(
    PrintFunc printFunc, const char* conditionStr, const std::string& message,
    const char* file, int line);

inline bool doAssert(
    bool condition, PrintFunc printFunc, const char* conditionStr, const std::string& message,
    const char* file, int line)
{
    if (!condition)
        assertionFailed(printFunc, conditionStr, message, file, line);
    return condition;
}

#define NX_KIT_DEBUG_DETAIL_CONCAT(X, Y) NX_KIT_DEBUG_DETAIL_CONCAT2(X, Y)
#define NX_KIT_DEBUG_DETAIL_CONCAT2(X, Y) X##Y

/** @param file Supply __FILE__. */
NX_KIT_API std::string printPrefix(const char* file);

class NX_KIT_API Timer
{
public:
    Timer(bool enabled, PrintFunc printFunc, const char* tag);
    ~Timer();
    void mark(const char* markStr);
    void finish();

private:
    struct Impl;
    Impl* const d;
};

class NX_KIT_API Fps
{
public:
    Fps(PrintFunc printFunc, const char* tag);
    ~Fps();
    void mark(const char* markStr = nullptr);

private:
    struct Impl;
    Impl* const d;
};

NX_KIT_API void printHexDump(
    PrintFunc printFunc, const char* caption, const char* bytes, int size);

} // namespace detail

} // namespace debug
} // namespace kit
} // namespace nx

#if defined(__linux__)
    #include <pthread.h>
    #define NX_KIT_DEBUG_DETAIL_THREAD_ID() \
        ::nx::kit::utils::format(", thread %llx", (long long) pthread_self())
#elif defined(QT_CORE_LIB)
    #include <QtCore/QThread>
    #define NX_KIT_DEBUG_DETAIL_THREAD_ID() \
        ::nx::kit::utils::format(", thread %llx", (long long) QThread::currentThreadId())
#else
    // No threading libs available - do not print thread id.
    #define NX_KIT_DEBUG_DETAIL_THREAD_ID() ""
#endif
