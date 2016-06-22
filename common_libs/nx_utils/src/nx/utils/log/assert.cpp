#include "log.h"
#include "assert.h"

namespace nx {
namespace utils {

void logError(const lm& message)
{
    NX_LOG(message, cl_logERROR);
}

#ifdef NX_ASSERT_MEASURE_TIME
    /** Change to see more or less records at the end of execution */
    static size_t kShowTheMostCount(30);

    /** Edit to change output sort order */
    static bool compareTheMost(
        const std::pair<size_t, std::chrono::microseconds>& left,
        const std::pair<size_t, std::chrono::microseconds>& right)
    {
        // sort by total time
        return left.second > right.second;
    }

    void AssertTimer::add(
        const char* file, int line,
        std::chrono::microseconds time)
    {
        std::ostringstream ss;
        ss << file << ":" << line;

        std::lock_guard<std::mutex> g(m_mutex);
        auto& current = m_times[ss.str()];
        current.first += 1;
        current.second += time;
    }

    AssertTimer::~AssertTimer()
    {
        typedef std::pair<std::string, std::pair<
            size_t, std::chrono::microseconds>> Record;

        std::lock_guard<std::mutex> g(m_mutex);
        std::vector<Record> sorted(m_times.begin(), m_times.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const Record& left, const Record& right)
            { return compareTheMost(left.second, right.second); });

        size_t toPrint = kShowTheMostCount;
        for (const auto& record: sorted)
        {
            const auto count = record.second.first;
            const auto time = record.second.second.count();

            std::cout << "AssertTimer( " << record.first << " ) called "
                << count << " times, total " << time << " ms, average "
                << (time / count) << " ms" << std::endl;

            if (--toPrint == 0)
                return;
        }
    }

    AssertTimer AssertTimer::instance;
#endif

} // namespace utils
} // namespace nx
