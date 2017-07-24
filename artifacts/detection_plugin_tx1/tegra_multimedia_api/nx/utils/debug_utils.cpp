#include "debug_utils.h"

#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>

#include "string.h"

#if defined(OUTPUT_PREFIX)
    #error OUTPUT_PREFIX should NOT be defined while compiling this .cpp file.
#endif // OUTPUT_PREFIX

namespace nx {
namespace utils {

#if defined(QT_CORE_LIB)
    QDebug operator<<(QDebug d, const std::string& s)
    {
        return d << s.c_str();
    }
#endif // QT_CORE_LIB

using nx::kit::debug::format;

long getTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t getTimeUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void logTimeUs(const char* outputPrefix, int64_t oldTimeUs, const char* tag)
{
    int64_t timeUs = getTimeUs();
    const char* const OUTPUT_PREFIX = outputPrefix; //< Will be used by PRINT macro.
    PRINT << "TIME us: " << (timeUs - oldTimeUs) << " [" << tag << "]";
}

struct DebugFps::Private
{
    static const int kFpsCount = 30;
    const char* outputPrefix;
    const char* tag;
    std::deque<uint64_t> deltaUsList;
    uint64_t prevTimeUs = 0;
};

DebugFps::DebugFps(bool enabled, const char* outputPrefix, const char* tag)
{
    if (enabled)
    {
        d.reset(new Private());
        d->outputPrefix = outputPrefix;
        d->tag = tag;
    }
}

DebugFps::~DebugFps() //< Destructor should be defined to satisfy unique_ptr.
{
}

void DebugFps::mark(const char* extraPrefix)
{
    if (d)
    {
        const uint64_t timeUs = getTimeUs();
        if (d->prevTimeUs != 0)
        {
            const uint64_t deltaUs = timeUs - d->prevTimeUs;
            d->deltaUsList.push_back(deltaUs);
            if ((int) d->deltaUsList.size() > d->kFpsCount)
                d->deltaUsList.pop_front();
            double deltaAvg = d->deltaUsList.empty() ? 0 :
                std::accumulate(d->deltaUsList.begin(), d->deltaUsList.end(), 0.0)
                / d->deltaUsList.size();
            const char* const OUTPUT_PREFIX = d->outputPrefix; //< Will be used by PRINT macro.
            PRINT << format("%s[%s] FPS avg %4.1f, dt %3d ms, avg dt %3d ms",
                extraPrefix ? extraPrefix : "",
                d->tag,
                1000000.0 / deltaAvg,
                int((500 + deltaUs) / 1000),
                int((500 + deltaAvg) / 1000));
        }
        d->prevTimeUs = timeUs;
    }
}

struct DebugTimer::Private
{
    const char* name;
    const char* outputPrefix;
    int64_t startTimeUs;
    std::vector<int64_t> list;
    std::vector<std::string> tags;
};

DebugTimer::DebugTimer(bool enabled, const char* outputPrefix, const char* name)
{
    if (enabled)
    {
        d.reset(new Private());
        d->outputPrefix = outputPrefix;
        d->name = name;
        d->startTimeUs = getTimeUs();
    }
}

DebugTimer::~DebugTimer() //< Destructor should be defined to satisfy unique_ptr.
{
}

void DebugTimer::mark(const char* tag)
{
    if (d)
    {
        const auto t = getTimeUs() - d->startTimeUs;
        if (std::find(d->tags.begin(), d->tags.end(), std::string(tag)) != d->tags.end())
        {
            const char* const OUTPUT_PREFIX = d->outputPrefix; //< Will be used by PRINT macro.
            PRINT << "INTERNAL ERROR: Timer mark '" << tag << "' already defined.";
        }
        else
        {
            d->list.push_back(t);
            d->tags.push_back(tag);
        }
    }
}

void DebugTimer::finish(const char* tag)
{
    if (d)
    {
        mark(tag);

        const auto t = getTimeUs() - d->startTimeUs;
        std::string s;
        for (int i = 0; i < (int) d->list.size(); ++i)
        {
            if (i > 0)
                s.append(", ");
            s.append(d->tags.at(i));
            s.append(format(": %3d ms", int((500 + d->list.at(i)) / 1000)));
        }
        if (d->list.size() > 0)
            s.append(", ");
        s.append(format("last: %3d ms", int((500 + t) / 1000)));
        const char* const OUTPUT_PREFIX = d->outputPrefix; //< Will be used by PRINT macro.
        PRINT << "TIME [" << d->name << "]: " << s;
    }
}

uint8_t* debugUnalignPtr(void* data)
{
    static const int kMediaAlignment = 32;
    return (uint8_t*) (
        (17 + ((uintptr_t) data) + kMediaAlignment - 1) / kMediaAlignment * kMediaAlignment);
}

namespace {

static const int kHexDumpBytesPerLine = 16;

static std::string hexDumpLine(const char* bytes, int size)
{
    if (size <= 0)
        return "";

    std::string result;

    // Print Hex.
    for (int i = 0; i < size; ++i)
    {
        if (i > 0)
            result.append(" ");
        result.append(format("%02X", bytes[i]));
    }
    // Print spaces to align "|" properly.
    for (int i = size; i < kHexDumpBytesPerLine; ++i)
        result.append(3, ' ');

    result.append(" | ");

    // Print ASCII chars.
    for (int i = 0; i < size; ++i)
    {
        if (bytes[i] >= 32 && bytes[i] <= 126) //< ASCII printable
            result.append(1, bytes[i]);
        else
            result.append(1, '.');
    }
    return result;
}

} // namespace

void debugPrintBin(const char* bytes, int size, const char* tag, const char* outputPrefix)
{
    const std::string header = format("Hex dump \"%s\" [%d]", tag, size);

    const char* const OUTPUT_PREFIX = outputPrefix; //< Will be used by PRINT macro.

    if (size <= 8) //< Print in single line.
    {
        PRINT << header << " { " << hexDumpLine(bytes, size) << " }";
    }
    else
    {
        PRINT << header;
        PRINT << "{";
        int count = size;
        const char* p = bytes;
        while (count > 0)
        {
            const int len = count >= kHexDumpBytesPerLine ? kHexDumpBytesPerLine : count;
            PRINT << "    " << hexDumpLine(p, len);
            p += len;
            count -= len;
        }
        PRINT << "}";
    }
}

} // namespace utils
} // namespace nx
