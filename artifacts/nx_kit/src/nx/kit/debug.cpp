// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug.h"

#include <chrono>
#include <deque>
#include <fstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <memory>
#include <map>
#include <mutex>
#include <cstring>

namespace nx {
namespace kit {
namespace debug {

using namespace nx::kit::utils;

//-------------------------------------------------------------------------------------------------
// Tools

std::string srcFileRelativePath(const std::string& file)
{
    // If the path contains "/src/nx/", return the path starting with "nx/".
    static const std::string srcPath = kPathSeparator + std::string("src") + kPathSeparator;
    const auto pos = file.find(srcPath + "nx" + kPathSeparator);
    if (pos != std::string::npos)
        return file.substr(pos + srcPath.size());

    const std::string thisFile = __FILE__;

    // Trim a prefix common with this cpp file.
    int p = 0;
    while (file[p] && file[p] == thisFile[p])
        ++p;
    return file.substr(p);
}

std::string srcFileBaseNameWithoutExt(const std::string& file)
{
    const size_t separatorPos = file.rfind(kPathSeparator);
    const int baseNamePos = (separatorPos != std::string::npos) ? ((int) separatorPos + 1) : 0;
    const size_t  extPos = file.rfind('.');
    if (extPos == std::string::npos)
        return file.substr(baseNamePos);
    return file.substr(baseNamePos, extPos - baseNamePos);
}

//-------------------------------------------------------------------------------------------------
// Output

std::ostream*& stream()
{
    static std::ostream* stream = &std::cerr;
    return stream;
}

namespace detail {

std::string printPrefix(const char* file)
{
    static std::mutex mutex;
    static std::map<std::string, std::string> cache;
    const std::lock_guard<std::mutex> lock(mutex);

    std::string& result = cache[file]; //< Ref to either a newly created or existing entry.
    if (result.empty()) //< Newly created entry.
        result = "[" + srcFileBaseNameWithoutExt(file) + "] ";
    return result;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Assertions

void intentionallyCrash(const char* message)
{
    #if defined(_WIN32)
        char messageOnStack[256] = {0};
        if (message)
            snprintf(messageOnStack, sizeof(messageOnStack), "%s", message);
    #else
        /*unused*/ (void) message;
    #endif

    // Crash the process to let a dump/core be generated.
    *reinterpret_cast<volatile int*>(0) = 0;
}

namespace detail {

void assertionFailed(
    PrintFunc printFunc, const char* conditionStr, const std::string& message,
    const char* file, int line)
{
    printFunc((std::string("\n")
        + ">>> ASSERTION FAILED: "
        + file + ":" + toString(line)
        + " (" + conditionStr + ") " + message
    ).c_str());

    #if !defined(NDEBUG)
        intentionallyCrash(message.c_str());
    #endif
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Print info

std::string hexDumpLine(const char* bytes, int size, int bytesPerLine)
{
    if (size <= 0)
        return "";

    std::string result;

    // Print Hex.
    for (int i = 0; i < size; ++i)
    {
        if (i > 0)
            result.append(" ");
        result.append(format("%02X", (unsigned char) bytes[i]));
    }
    // Print spaces to align "|" properly.
    for (int i = size; i < bytesPerLine; ++i)
        result.append(3, ' ');

    result.append(" | ");

    // Print ASCII chars.
    for (int i = 0; i < size; ++i)
    {
        if (isAsciiPrintable(bytes[i]))
            result.append(1, bytes[i]);
        else
            result.append(1, '.');
    }
    return result;
}

namespace detail {

static constexpr int kHexDumpBytesPerLine = 16;

void printHexDump(
    PrintFunc printFunc, const char* caption, const char* const bytes, int size)
{
    std::string s = format("####### Hex dump \"%s\", %d bytes @%p:", caption, size, bytes);

    if (size <= 8) //< Print in single line.
    {
        printFunc((s + " { " + hexDumpLine(bytes, size, /*bytesPerLine*/ 0) + " }").c_str());
        return;
    }

    s += "\n{";
    int count = size;
    const char* p = bytes;
    while (count > 0)
    {
        const int len = count >= kHexDumpBytesPerLine ? kHexDumpBytesPerLine : count;
        s += "\n    ";
        s += hexDumpLine(p, len, kHexDumpBytesPerLine);
        p += len;
        count -= len;
    }
    s += "\n}";
    printFunc(s.c_str());
}

void saveStr(
    PrintFunc printFunc,
    const char* originDir,
    const char* filename,
    const char* strCaption,
    const std::string& str)
{
    const std::string effectiveFilename = nx::kit::utils::absolutePath(originDir, filename);

    std::ofstream file(effectiveFilename, std::ios::binary); //< Binary to avoid CR+LF on Windows.
    if (!file.good())
    {
        printFunc(("####### ERROR: Unable to rewrite file " + effectiveFilename).c_str());
        return;
    }

    printFunc(("####### Saving string (" + std::string(strCaption) + ") to file "
        + effectiveFilename).c_str());

    file.write(str.c_str(), str.size()); //< Support '\0' inside the string.
}

void saveBin(
    PrintFunc printFunc,
    const char* originDir,
    const char* filename,
    const char* bytes,
    int size)
{
    const std::string effectiveFilename = nx::kit::utils::absolutePath(originDir, filename);

    std::ofstream file(effectiveFilename, std::ios::binary);
    if (!file.good())
    {
        printFunc(("####### ERROR: Unable to rewrite file " + effectiveFilename).c_str());
        return;
    }

    printFunc(nx::kit::utils::format("####### Saving %d byte(s) to file %s",
        size, effectiveFilename.c_str()).c_str());

    file.write(bytes, size);
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Time

namespace detail {

static int64_t getTimeUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

struct Timer::Impl
{
    const char* tag;
    PrintFunc printFunc;
    int64_t startTimeUs;
    std::vector<int64_t> times;
    std::vector<std::string> marks;

    Impl(PrintFunc printFunc, const char* const tag):
        tag(tag),
        printFunc(printFunc),
        startTimeUs(getTimeUs())
    {
    }
};

Timer::Timer(bool enabled, PrintFunc printFunc, const char* const tag):
    d(enabled ? new Impl(printFunc, tag) : nullptr)
{
}

Timer::~Timer()
{
    delete d;
}

void Timer::mark(const char* markStr)
{
    if (!d)
        return;

    const auto t = getTimeUs() - d->startTimeUs;
    if (std::find(d->marks.begin(), d->marks.end(), std::string(markStr)) != d->marks.end())
    {
        d->printFunc((std::string("####### NX_TIME(") + d->tag + ") INTERNAL ERROR: Timer mark \""
            + markStr + "\" already defined.").c_str());
    }
    else
    {
        d->times.push_back(t);
        d->marks.push_back(std::string(markStr));
    }
}

void Timer::finish()
{
    if (!d)
        return;

    const auto t = getTimeUs() - d->startTimeUs;
    std::string s;
    if (d->marks.empty())
    {
        s = format("%d us", (int) t);
    }
    else //< If there were marks, print "ms" instead of "us" for readability.
    {
        for (size_t i = 0; i < d->times.size(); ++i)
        {
            s.append(format("%s: %3d ms, ",
                d->marks.at(i).c_str(), (int) ((500 + d->times.at(i)) / 1000)));
        }
        s.append(format("last: %3d ms", (int) ((500 + t) / 1000)));
    }

    d->printFunc((std::string("####### NX_TIME(") + d->tag + "): " + s).c_str());
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Fps

namespace detail {

struct Fps::Impl
{
    static const int kMaxStoredMarks = 30;
    PrintFunc printFunc;
    const char* tag;
    std::deque<int64_t> deltaUsList;
    int64_t prevTimeUs = 0;
};

Fps::Fps(PrintFunc printFunc, const char* const tag):
    d(new Impl())
{
    d->printFunc = printFunc;
    d->tag = tag;
}

Fps::~Fps()
{
    delete d;
}

void Fps::mark(const char* markStr /*= nullptr */)
{
    const int64_t timeUs = getTimeUs();
    if (d->prevTimeUs != 0)
    {
        const int64_t deltaUs = timeUs - d->prevTimeUs;
        d->deltaUsList.push_back(deltaUs);
        if ((int) d->deltaUsList.size() > d->kMaxStoredMarks)
            d->deltaUsList.pop_front();
        double deltaAvg = d->deltaUsList.empty() ? 0 :
            std::accumulate(d->deltaUsList.begin(), d->deltaUsList.end(), 0.0)
                / d->deltaUsList.size();
        d->printFunc(format("####### NX_FPS(%s): avg %4.1f, dt %3d ms, avg dt %3d ms%s",
            d->tag,
            1000000.0 / deltaAvg,
            int((500 + deltaUs) / 1000),
            int((500 + deltaAvg) / 1000),
            markStr ? (std::string("; ") + markStr).c_str() : ""
        ).c_str());
    }
    d->prevTimeUs = timeUs;
}

} // namespace detail

} // namespace debug
} // namespace kit
} // namespace nx
