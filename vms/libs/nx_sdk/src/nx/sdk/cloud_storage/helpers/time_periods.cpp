// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_periods.h"

namespace nx::sdk::cloud_storage {

TimePeriods::TimePeriods(const TimePeriodList& periods)
{
    setPeriods(periods);
}

void TimePeriods::goToBeginning() const
{
    m_it = m_periods.cbegin();
}

bool TimePeriods::next() const
{
    const bool result = m_it != m_periods.cend();
    if (result)
        ++m_it;

    return result;
}

bool TimePeriods::atEnd() const
{
    return m_it == m_periods.cend();
}

bool TimePeriods::get(int64_t* outStartUs, int64_t* outEndUs) const
{
    if (atEnd())
        return false;

    using namespace std::chrono;
    *outStartUs = duration_cast<milliseconds>(m_it->startTimestamp).count() * 1000;
    *outEndUs = *outStartUs + m_it->duration.count() * 1000;

    return true;
}

void TimePeriods::setPeriods(const TimePeriodList& periods)
{
    periods.forEach([this](const auto& p) { m_periods.push_back(p); });
    m_it = m_periods.cbegin();
}

} // namespace nx::sdk::cloud_storage
