#pragma once

// This unit allows to be compiled in context of both nx_vms and any independent project.
// Requirements:
// #define OUTPUT_PREFIX "..."
// bool conf.enableTime;
// bool conf.enableFps;

#include <memory>
#include <cstdint>
#include <cstring>

#if defined(QT_CORE_LIB)
    #include <QtCore/QDebug>
#else // QT_CORE_LIB
    #include <iostream>
#endif // QT_CORE_LIB

namespace nx {
namespace utils {

// TODO: Consider renaming PRINT and OUTPUT. Consider introducing namespace 'debug'.

namespace {

/** @return Part of a source code filename which is a path relative to "nx_vms..." folder. */
static inline const char* relative_src_filename(const char* s)
{
    /*unused*/ (void) relative_src_filename;
    auto pos = std::string(__FILE__).find("common_libs"); //< This file resides in "common_libs".
    if (pos != std::string::npos && pos < strlen(s) && strncmp(s, __FILE__, pos) == 0)
        return s + pos;
    return s; //< Use original file name.
}

} // namespace

#if defined(QT_CORE_LIB)

    // TODO: Rewrite using log.h
    NX_UTILS_API QDebug operator<<(QDebug d, const std::string& s);
    #define PRINT qWarning().nospace() << OUTPUT_PREFIX

    #define LL qDebug().nospace() << "####### line " << __LINE__ \
        << " [" << nx::utils::relative_src_filename(__FILE__) << "]";

#else // QT_CORE_LIB

    namespace {
        struct EndWithEndl { ~EndWithEndl() { std::cerr << std::endl; } };
    } // namespace
    #define PRINT nx::utils::EndWithEndl() /*operator,*/, std::cerr << OUTPUT_PREFIX

    // Log execution of a line - to use, put double-L at the beginning of the line.
    #define LL std::cerr << "####### line " << __LINE__ \
        << " [" << nx::utils::relative_src_filename(__FILE__) << "]\n";

#endif // QT_CORE_LIB

#define OUTPUT if (!conf.enableOutput) {} else PRINT

/**
 * Is a stub if !conf.enableFps.
 */
class NX_UTILS_API DebugFps
{
public:
    DebugFps(bool enabled, const char* outputPrefix, const char* tag);
    ~DebugFps();
    void mark(const char* extraPrefix = nullptr);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
#define NX_SHOW_FPS(TAG, /*PREFIX*/...) do \
{ \
    if (conf.enableFps) \
    { \
        static nx::utils::DebugFps debugFps(conf.enableFps, OUTPUT_PREFIX, TAG); \
        debugFps.mark(__VA_ARGS__); \
    } \
} while (0)

NX_UTILS_API long getTimeMs();
NX_UTILS_API int64_t getTimeUs();
NX_UTILS_API void logTimeUs(const char* outputPrefix, int64_t oldTimeUs, const char* tag);
#define NX_TIME_BEGIN(TAG) int64_t TIME_##TAG = conf.enableTime ? nx::utils::getTimeUs() : 0
#define NX_TIME_END(TAG) if(conf.enableTime) nx::utils::logTimeUs(OUTPUT_PREFIX, TIME_##TAG, #TAG)

/**
 * Is a stub if !conf.enableTime.
 */
class NX_UTILS_API DebugTimer
{
public:
    DebugTimer(bool enabled, const char* outputPrefix, const char* name);
    ~DebugTimer();
    void mark(const char* tag);
    void finish(const char* tag);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
#define NX_TIMER_CREATE(NAME) nx::utils::DebugTimer TIMER_(conf.enableTime, OUTPUT_PREFIX, (NAME))
#define NX_TIMER_MARK(TAG) TIMER_.mark(TAG)
#define NX_TIMER_FINISH(TAG) TIMER_.finish(TAG)

/** Debug tool - implicitly unaligned pointer. */
NX_UTILS_API uint8_t* debugUnalignPtr(void* data);

/** Debug tool - dump binary data using PRINT. */
void debugPrintBin(const char* bytes, int size, const char* tag, const char* outputPrefix);
#define NX_PRINT_BIN(BYTES, SIZE, TAG) nx::utils::debugPrintBin((BYTES), (SIZE), (TAG), (OUTPUT_PREFIX))

} // namespace utils
} // namespace nx
