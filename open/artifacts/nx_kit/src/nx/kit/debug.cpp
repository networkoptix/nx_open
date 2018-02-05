// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#include "debug.h"

#include <chrono>
#include <deque>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cstdarg>
#include <memory>
#include <map>
#include <cstring>

namespace nx {
namespace kit {
namespace debug {

//-------------------------------------------------------------------------------------------------
// Tools

uint8_t* unalignedPtr(void* data)
{
    static const int kMediaAlignment = 32;
    return (uint8_t*) (
        17 + (((uintptr_t) data) + kMediaAlignment - 1) / kMediaAlignment * kMediaAlignment);
}

std::string format(const std::string formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    int size = vsnprintf(nullptr, 0, formatStr.c_str(), args) + 1; //< Extra space for '\0'.
    if (size < 0)
    {
        va_end(args);
        return formatStr; //< No better way to handle out-of-memory-like errors.
    }
    std::unique_ptr<char[]> buf(new char[size]);
    va_end(args);

    va_start(args, formatStr);
    vsnprintf(buf.get(), (size_t) size, formatStr.c_str(), args);
    va_end(args);
    return std::string(buf.get(), buf.get() + size - 1); //< Trim trailing '\0'.
}

bool isAsciiPrintable(int c)
{
    return c >= 32 && c <= 126;
}

char pathSeparator()
{
    static const char separator =
        (char) (strchr(__FILE__, '/') ? '/' : (strchr(__FILE__, '\\') ? '\\' : '\0'));
    return separator;
}

size_t commonPrefixSize(const std::string& s1, const std::string& s2)
{
    const std::string& shorter = (s1.size() < s2.size()) ? s1 : s2;
    const std::string& longer = (s1.size() < s2.size()) ? s2 : s1;

    const auto afterCommonPrefix = std::mismatch(
        shorter.cbegin(), shorter.cend(), longer.cbegin());

    return (size_t) (afterCommonPrefix.first - shorter.cbegin());
}

const char* relativeSrcFilename(const char* file)
{
    if (!pathSeparator()) //< Unable to detect path separator - return original file name.
        return file;

    const std::string fileString = file;

    // If the path contains "/src/nx/", return starting with "nx/".
    {
        static const std::string srcPath = pathSeparator() + std::string("src") + pathSeparator();
        const auto pos = fileString.find(srcPath + "nx" + pathSeparator());
        if (pos != std::string::npos)
            return file + pos + srcPath.size();
    }

    // Trim a prefix common with this cpp file, and one subsequent directory (if any).
    {
        size_t filesCommonPrefixSize = commonPrefixSize(__FILE__, fileString);
        if (filesCommonPrefixSize == 0) //< No common prefix - return the original file.
            return file;

        const auto pos = fileString.find(pathSeparator(), filesCommonPrefixSize);
        if (pos == std::string::npos) //< No separator - return file after the common prefix.
             return file + filesCommonPrefixSize;
        return file + pos + sizeof(pathSeparator());
    }
}

std::string fileBaseNameWithoutExt(const char* file)
{
    const char* const sep = pathSeparator() ? strrchr(file, pathSeparator()) : nullptr;
    const char* const first = sep ? sep + 1 : file;
    const char* const ext = strrchr(file, '.');
    if (ext)
        return std::string(first, ext - first);
    else
        return std::string(first);
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
    static std::map<std::string, std::string> cache;

    auto& result = cache[file]; //< Ref to either newly created or existing entry.
    if (result.empty()) //< Newly created entry.
        result = "[" + fileBaseNameWithoutExt(file) + "] ";
    return result;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Print info

std::string toString(std::string s)
{
    return toString(s.c_str());
}

std::string toString(char c)
{
    if (!isAsciiPrintable(c))
        return format("'\\x%02X'", (unsigned char) c);
    return std::string("'") + c + "'";
}

std::string toString(const char* s)
{
    if (s == nullptr)
        return "null";

    std::string str;
    for (const char* p = s; *p != '\0'; ++p)
    {
        if (*p == '\\' || *p == '"')
            (str += '\\') += *p;
        else if (*p == '\n')
            str += "\\n";
        else if (*p == '\t')
            str += "\\t";
        else if (!isAsciiPrintable(*p))
            str.append(format("\\x%02X", (unsigned char) *p));
        else
            str += *p;
    }
    return "\"" + str + "\"";
}

std::string toString(char* s)
{
    return toString(const_cast<const char*>(s));
}

std::string toString(const void* ptr)
{
    if (ptr == nullptr)
        return "null";
    return format("%p", ptr);
}

namespace detail {

/**
 * @param bytesPerLine Used to calculate space padding, 0 means no padding.
 */
static std::string hexDumpLine(const char* bytes, int size, int bytesPerLine)
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

static constexpr int kHexDumpBytesPerLine = 16;

void printHexDump(
    PrintFunc printFunc, const char* caption, const char* const bytes, int size)
{
    const std::string header = format("Hex dump \"%s\", %d bytes:", caption, size);

    if (size <= 8) //< Print in single line.
    {
        printFunc((header + " { " + hexDumpLine(bytes, size, /*bytesPerLine*/ 0) + " }").c_str());
    }
    else
    {
        printFunc(header.c_str());
        printFunc("{");
        int count = size;
        const char* p = bytes;
        while (count > 0)
        {
            const int len = count >= kHexDumpBytesPerLine ? kHexDumpBytesPerLine : count;
            printFunc((std::string("    ") + hexDumpLine(p, len, kHexDumpBytesPerLine)).c_str());
            p += len;
            count -= len;
        }
        printFunc("}");
    }
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
        d->printFunc((std::string("NX_TIME(") + d->tag + ") INTERNAL ERROR: Timer mark \""
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

    d->printFunc((std::string("NX_TIME(") + d->tag + "): " + s).c_str());
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
        d->printFunc(format("NX_FPS(%s): avg %4.1f, dt %3d ms, avg dt %3d ms%s",
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
