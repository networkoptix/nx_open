#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace utils {

template<typename Result>
class ResultCounter
{
public:
    typedef std::map<Result, size_t> Counters;
    typedef MoveOnlyFunc<QString(const Result&)> StringConvertor;

    explicit ResultCounter(StringConvertor stringConvertor, bool isEnabled = false);

    void enable(bool isEnabled = true);
    void addResult(const Result& result);

    Counters getCounters() const;
    QString stringStatus() const;

private:
    const StringConvertor m_stringConvertor;
    std::atomic<bool> m_isEnabled;

    mutable QnMutex m_mutex;
    Counters m_counters;
};

// ------------------------------------------------------------------------------------------------

template<typename Result>
ResultCounter<Result>::ResultCounter(StringConvertor stringConvertor, bool isEnabled):
    m_stringConvertor(std::move(stringConvertor)),
    m_isEnabled(isEnabled)
{
}

template<typename Result>
void ResultCounter<Result>::enable(bool isEnabled)
{
    m_isEnabled = isEnabled;
    if (!isEnabled)
    {
        QnMutexLocker lock(&m_mutex);
        m_counters.clear();
    }
}

template<typename Result>
void ResultCounter<Result>::addResult(const Result& result)
{
    if (!m_isEnabled)
        return;

    QnMutexLocker lock(&m_mutex);
    m_counters.emplace(result, 0).first->second += 1;
}

template<typename Result>
typename ResultCounter<Result>::Counters ResultCounter<Result>::getCounters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_counters;
}

template<typename Result>
QString ResultCounter<Result>::stringStatus() const
{
    QStringList returnCodes;
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& c: m_counters)
            returnCodes << lm("%1 [%2 time(s)]").args(m_stringConvertor(c.first), c.second);
    }

    return returnCodes.join(QLatin1Literal("; "));
}

} // namespace utils
} // namespace nx
