// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "assert.h"

#if defined(_WIN32)
#else
    #include <signal.h>
    #include <unistd.h>
    #include <pthread.h>
#endif

#include <nx/utils/stack_trace.h>

#include "log.h"

namespace nx::utils {

namespace {

/** Change to see more or less records at the end of execution */
static const size_t kShowTheMostCount(30);

static std::function<void(const QString&)> g_onAssertHandler;

static bool g_printStackTraceOnAssert = false;

} // namespace

void setPrintStackTraceOnAssertEnabled(bool value)
{
    g_printStackTraceOnAssert = value;
}

void setOnAssertHandler(std::function<void(const QString&)> handler)
{
    g_onAssertHandler = std::move(handler);
}

void crashProgram([[maybe_unused]] const QString& message)
{
    #if defined(_WIN32)
        // Copy error text to a stack variable so it is present in the mini-dump.
        char textOnStack[256] = {0};
        const auto text = nx::format("%1").arg(message).toStdString();
        snprintf(textOnStack, sizeof(textOnStack), "%s", text.c_str());
    #endif

    #if defined(_DEBUG)
        #if defined(_WIN32)
            *reinterpret_cast<volatile int*>(0) = 7;
        #else
            pthread_kill(pthread_self(), SIGTRAP);
        #endif
    #else
        *reinterpret_cast<volatile int*>(0) = 7;
    #endif
}

bool assertFailure(bool isCritical, const QString& message)
{
    static const log::Tag kCrashTag(QLatin1String("CRASH"));
    static const log::Tag kAssertTag(QLatin1String("ASSERT"));

    const bool isCrashRequired = isCritical || ini().assertCrash;
    const auto& kTag = isCrashRequired ? kCrashTag : kAssertTag;
    const auto output = g_printStackTraceOnAssert || ini().printStackTraceOnAssert
        ? NX_FMT("%1\n\n%2", message, stackTrace()).toQString()
        : message;
    NX_ERROR(kTag, output);
    std::cerr << std::endl << ">>> " << output.toStdString() << std::endl;

    if (g_onAssertHandler)
        g_onAssertHandler(message);

    if (isCrashRequired)
        crashProgram(message);

    return false;
}

static QtMessageHandler g_defaultQtMessageHandler = 0;

static void handleQtMessage(
    QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // Catch invalid number of arguments, invalid numeric translations, invalid old-style connects
    // and some metatype errors.
    if (message.contains("QString:")
        || message.contains("QObject:")
        || message.contains("unable to save")
        || message.contains("unknown user type")
        || message.contains("QCharRef with an index pointing outside the valid range of a QString")
        || message.contains("behavior is deprecated")
        || message.contains("destroying locked mutex"))
    {
        assertFailure(/*isCritical*/ false, message);
    }
    else
    {
        switch (type)
        {
            case QtWarningMsg:
            case QtCriticalMsg:
                NX_WARNING(&context, message);
                break;

            case QtInfoMsg:
                NX_INFO(&context, message);
                break;

            case QtDebugMsg:
                NX_VERBOSE(&context, message);
                break;

            default: // QtFatalMsg and all unknown.
                assertFailure(/*isCritical*/ false, message);
                break;
        };
    }

    if (g_defaultQtMessageHandler)
        g_defaultQtMessageHandler(type, context, message);
}

void enableQtMessageAsserts()
{
    if (g_defaultQtMessageHandler)
        NX_ASSERT(false, "Qt message asserts are already enabled");
    else
        g_defaultQtMessageHandler = qInstallMessageHandler(handleQtMessage);

}

void disableQtMessageAsserts()
{
    if (g_defaultQtMessageHandler)
        qInstallMessageHandler(g_defaultQtMessageHandler);
    else
        NX_ASSERT(false, "Qt message asserts are not enabled");
}

AssertTimer::TimeInfo::TimeInfo():
    m_count(0),
    m_time(0)
{
}

AssertTimer::TimeInfo::TimeInfo(const TimeInfo& other):
    m_count(other.m_count.load()),
    m_time(other.m_time.load())
{
}

AssertTimer::TimeInfo& AssertTimer::TimeInfo::operator=(const TimeInfo& other)
{
    m_count = other.m_count.load();
    m_time = other.m_time.load();
    return *this;
}

/** Edit to change output sort order */
bool AssertTimer::TimeInfo::operator<(const TimeInfo& other) const
{
    return m_time > other.m_time;
}

void AssertTimer::TimeInfo::add(std::chrono::microseconds duration)
{
    m_count += 1;
    m_time += duration.count();
}

size_t AssertTimer::TimeInfo::count() const
{
    return m_count;
}

std::chrono::microseconds AssertTimer::TimeInfo::time() const
{
    return std::chrono::microseconds(m_time);
}

AssertTimer::TimeInfo* AssertTimer::info(const char* file, int line)
{
    std::stringstream ss;
    ss << file << ":" << line;

    std::lock_guard<std::mutex> g(m_mutex);
    return &m_times[ss.str()];
}

AssertTimer::~AssertTimer()
{
    std::lock_guard<std::mutex> g(m_mutex);
    typedef std::pair<std::string, TimeInfo> Record;

    std::vector<Record> sorted(m_times.begin(), m_times.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const Record& a, const Record& b) { return a.second < b.second; });

    size_t toPrint = kShowTheMostCount;
    for (const auto& record: sorted)
    {
        const auto count = record.second.count();
        const auto time = record.second.time();

        typedef decltype(time)::period ratio;
        const auto timeS = double(time.count()) * ratio::num / ratio::den;

        std::cout << "AssertTimer( " << record.first << " ) called "
            << count << " times, total "<< timeS << " s, average "
            << (timeS / count) << " s" << std::endl;

        if (--toPrint == 0)
            return;
    }
}

#ifdef NX_CHECK_MEASURE_TIME
    AssertTimer AssertTimer::instance;
#endif

} // namespace nx::utils
