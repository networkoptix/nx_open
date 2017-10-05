// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#pragma once

/**@file
 * Utilities for debugging: measuring execution time and FPS, working with strings, logging values
 * and messages.
 *
 * This unit can be compiled in the context of any C++ project. If Qt headers are included before
 * this one, some Qt support is enabled via "#if defined(QT_CORE_LIB)".
 */

#include <iostream>
#include <cstdint>
#include <functional>
#include <sstream>
#include <memory>

#if defined(QT_CORE_LIB)
    // To be supported in NX_PRINT_VALUE.
    #include <QtCore/QByteArray>
    #include <QtCore/QString>
    #include <QtCore/QUrl>
#endif

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {
namespace debug {

//-------------------------------------------------------------------------------------------------
// Tools

NX_KIT_API uint8_t* unalignedPtr(void* data);

NX_KIT_API std::string format(std::string formatStr, ...);

NX_KIT_API bool isAsciiPrintable(int c);

/** @return Separator in __FILE__ - slash or backslash, or '\0' if unknown. Cached in static. */
NX_KIT_API char pathSeparator();

NX_KIT_API size_t commonPrefixSize(const std::string& s1, const std::string& s2);

/**
 * Extract part of the source code filename which follows the uninteresting common part.
 * @return Pointer to some char inside the given string.
 */
NX_KIT_API const char* relativeSrcFilename(const char* file);

NX_KIT_API std::string fileBaseNameWithoutExt(const char* file);

//-------------------------------------------------------------------------------------------------
// Output

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
    #define NX_PRINT_PREFIX nx::kit::debug::detail::printPrefix(__FILE__)
#endif

#if !defined(NX_DEBUG_STREAM)
    /** Redefine if needed; used for all output by other macros. */
    #define NX_DEBUG_STREAM *nx::kit::debug::stream()
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

/**
 * Print the args to NX_DEBUG_STREAM, starting with NX_PRINT_PREFIX and ending with NX_DEBUG_ENDL.
 */
#define NX_PRINT /* << args... */ \
    /* Allocate a temp value, which prints endl in its destructor, in the "<<" expression. */ \
    ( []() { struct Endl { ~Endl() { NX_DEBUG_STREAM NX_DEBUG_ENDL; } }; \
        return std::make_shared<Endl>(); }() ) /*operator,*/, \
    NX_DEBUG_STREAM << NX_PRINT_PREFIX

/**
 * Print the args like NX_PRINT; do nothing if !NX_DEBUG_ENABLE_OUTPUT.
 */
#define NX_OUTPUT /* << args... */ \
    if (!(NX_DEBUG_ENABLE_OUTPUT)) {} else NX_PRINT

//-------------------------------------------------------------------------------------------------
// Print info

/**
 * Log execution of a line - to use, write LL at the beginning of the line.
 */
#define LL \
    NX_PRINT << "####### LL line " << __LINE__ \
        << NX_KIT_DEBUG_DETAIL_THREAD_ID() \
        << ", file " << nx::kit::debug::relativeSrcFilename(__FILE__);

/**
 * Print the expression text and its value in brackets.
 */
#define NX_PRINT_VALUE(VALUE) \
    NX_PRINT << "####### " #VALUE ": " << nx::kit::debug::detail::toString((VALUE))

/**
 * Hex-dump binary data using NX_PRINT.
 */
#define NX_PRINT_HEX_DUMP(TAG, BYTES, SIZE) \
    nx::kit::debug::detail::printHexDump( \
        NX_KIT_DEBUG_DETAIL_PRINT_FUNC, (TAG), (const char*) (BYTES), (int) (SIZE))

//-------------------------------------------------------------------------------------------------
// Time

#if !defined(NX_DEBUG_ENABLE_TIME)
    /** Redefine if needed. */
    #define NX_DEBUG_ENABLE_TIME NX_DEBUG_INI enableTime
#endif

/**
 * Start measuring time - save current time in a variable; do nothing if !NX_DEBUG_ENABLE_TIME.
 */
#define NX_TIME_BEGIN(TAG) \
    nx::kit::debug::detail::Timer nxTimer_##TAG( \
        (NX_DEBUG_ENABLE_TIME), NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #TAG)

/**
 * Measure the time in the intermediate point, to be printed at the end; do nothing if
 * !NX_DEBUG_ENABLE_TIME. If called at least once, forces "ms" instead of "us" for the totals.
 */
#define NX_TIME_MARK(TAG, MARK) do \
{ \
    if (NX_DEBUG_ENABLE_TIME) \
        nxTimer_##TAG.mark((MARK)); \
} while (0)

/**
 * Print the totals of this time measurement with NX_PRINT; do nothing if !NX_DEBUG_ENABLE_TIME.
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
 * Measure and print with NX_PRINT each period between executions of the point of call; do nothing
 * if !NX_DEBUG_ENABLE_FPS##TAG.
 */
#define NX_FPS(TAG, /*OPTIONAL_MARK*/...) do \
{ \
    if (NX_KIT_DEBUG_DETAIL_CONCAT(NX_DEBUG_ENABLE_FPS, TAG)) \
    { \
        static nx::kit::debug::detail::Fps fps( \
            NX_KIT_DEBUG_DETAIL_PRINT_FUNC, #TAG); \
        fps.mark(__VA_ARGS__); \
    } \
} while (0)

//-------------------------------------------------------------------------------------------------
// Implementation

namespace detail {

#define NX_KIT_DEBUG_DETAIL_CONCAT(X, Y) NX_KIT_DEBUG_DETAIL_CONCAT2(X, Y)
#define NX_KIT_DEBUG_DETAIL_CONCAT2(X, Y) X##Y

template<typename T>
std::string toString(T value)
{
    std::ostringstream outputString;
    outputString << value;
    return outputString.str();
}

NX_KIT_API std::string toString(std::string s);
NX_KIT_API std::string toString(char c);
NX_KIT_API std::string toString(const char* s);
NX_KIT_API std::string toString(char* s);
NX_KIT_API std::string toString(const void* ptr);

#if defined(QT_CORE_LIB)

static inline std::string toString(const QByteArray& b)
{
    return toString(b.toStdString());
}

static inline std::string toString(const QString& s)
{
    return toString(s.toUtf8().constData());
}

static inline std::string toString(const QUrl& u)
{
    return toString(u.toEncoded().toStdString());
}

#endif // defined(QT_CORE_LIB)

template<typename P>
std::string toString(P* ptr)
{
    return toString((const void*) ptr);
}

typedef std::function<void(const char*)> PrintFunc;

#define NX_KIT_DEBUG_DETAIL_PRINT_FUNC [](const char* message) { NX_PRINT << message; }

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
        nx::kit::debug::format(", thread %llx", (long long) pthread_self())
#elif defined(QT_CORE_LIB)
    #include <QtCore/QThread>
    #define NX_KIT_DEBUG_DETAIL_THREAD_ID() \
        nx::kit::debug::format(", thread %llx", (long long) QThread::currentThreadId())
#else
    // No threading libs available - do not print thread id.
    #define NX_KIT_DEBUG_DETAIL_THREAD_ID() ""
#endif
