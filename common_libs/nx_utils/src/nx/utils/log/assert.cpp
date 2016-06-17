#include "log.h"
#include "assert.h"

namespace nx {
namespace utils {

void logError(const lm& message)
{
    NX_LOG(message, cl_logERROR);
}

#ifdef NX_ASSERT_MEASURE_TIME
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
        std::lock_guard<std::mutex> g(m_mutex);
        typedef std::pair<std::string, std::pair<
            size_t, std::chrono::microseconds>> Record;

        std::vector<Record> sorted(m_times.begin(), m_times.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const Record& a, const Record& b)
            { return a.second.second > b.second.second; });

        size_t toPrint = NX_ASSERT_MEASURE_TIME;
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
