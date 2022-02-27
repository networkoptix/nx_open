// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include "string.h"

namespace nx::utils {

template<typename Result>
class ResultCounter
{
public:
    typedef std::map<Result, size_t> Counters;
    typedef MoveOnlyFunc<std::string(const Result&)> StringConvertor;

    template<typename StringConvertorFunc>
    ResultCounter(StringConvertorFunc func, bool isEnabled = false);

    void enable(bool isEnabled = true);
    void addResult(const Result& result);

    Counters getCounters() const;
    std::string stringStatus() const;

private:
    const StringConvertor m_stringConvertor;
    std::atomic<bool> m_isEnabled;

    mutable nx::Mutex m_mutex;
    Counters m_counters;
};

//-------------------------------------------------------------------------------------------------

template<typename Result>
template<typename StringConvertorFunc>
ResultCounter<Result>::ResultCounter(StringConvertorFunc func, bool isEnabled):
    m_stringConvertor(
        [func = std::move(func)](const Result& value)
        {
            return nx::format("%1").args(func(value)).toStdString();
        }),
    m_isEnabled(isEnabled)
{
}

template<typename Result>
void ResultCounter<Result>::enable(bool isEnabled)
{
    m_isEnabled = isEnabled;
    if (!isEnabled)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_counters.clear();
    }
}

template<typename Result>
void ResultCounter<Result>::addResult(const Result& result)
{
    if (!m_isEnabled)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_counters.emplace(result, 0).first->second += 1;
}

template<typename Result>
typename ResultCounter<Result>::Counters ResultCounter<Result>::getCounters() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_counters;
}

template<typename Result>
std::string ResultCounter<Result>::stringStatus() const
{
    std::vector<std::string> returnCodes;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const auto& c: m_counters)
        {
            returnCodes.push_back(
                nx::format("%1 [%2 times]").args(m_stringConvertor(c.first), c.second).toStdString());
        }
    }

    return nx::utils::join(returnCodes.begin(), returnCodes.end(), "; ");
}

} // namespace nx::utils
