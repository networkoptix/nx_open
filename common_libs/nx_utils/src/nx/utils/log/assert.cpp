#include "log.h"
#include "assert.h"

namespace nx {
namespace utils {

/** Change to see more or less records at the end of execution */
static size_t kShowTheMostCount(30);

void logError(const lm& message)
{
    NX_LOG(message, cl_logERROR);
}

AssertTimer::TimeInfo::TimeInfo()
:
    m_count(0),
    m_time(0)
{
}

AssertTimer::TimeInfo::TimeInfo(const TimeInfo& other)
:
    m_count(other.m_count.load()),
    m_time(other.m_time.load())
{
}

AssertTimer::TimeInfo& AssertTimer::TimeInfo::operator =(const TimeInfo& other)
{
    m_count = other.m_count.load();
    m_time = other.m_time.load();
    return *this;
}

/** Edit to change output sort order */
bool AssertTimer::TimeInfo::operator <(const TimeInfo& other) const
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

} // namespace utils
} // namespace nx
