// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <optional>

#include <nx/utils/elapsed_timer.h>

#include "average_per_period.h"

namespace nx::utils::math {

/**
 * Detects abnormal numeric value in a sequence of values and invokes a functor.
 */
template<
    typename Value,
    typename Multiplier = Value,
    typename... AuxReportArgs
>
class AbnormalValueDetector
{
public:
    using ReportAnomalyFunc =
        std::function<void(Value abnormalValue, Value currentAverage, AuxReportArgs...)>;

    /**
     * @param multipler Some value is considered abnormal if it is greater (if multipler >= 1)
     * or smaller (if multipler < 1) than an average value.
     * The average is taken for the last period.
     */
    AbnormalValueDetector(
        Multiplier multiplier,
        std::chrono::milliseconds period,
        ReportAnomalyFunc reportAnomalyFunc)
        :
        m_multiplier(multiplier),
        m_averageCalculator(period),
        m_period(period),
        m_reportAnomalyFunc(std::move(reportAnomalyFunc))
    {
    }

    /**
     * NOTE: Anomality detection really starts after accumulating data for the period
     * specified in the constructor.
     */
    template<typename... AuxArgs>
    void add(Value value, AuxArgs&&... args)
    {
        if (m_hasEnoughData)
        {
            testValue(value, std::forward<AuxArgs>(args)...);
        }
        else
        {
            if (!m_initialDataAccumulationTimer)
                m_initialDataAccumulationTimer = ElapsedTimer(ElapsedTimerState::started);

            if (m_initialDataAccumulationTimer->elapsed() > m_period)
                m_hasEnoughData = true;
        }

        m_averageCalculator.add(value);
    }

    Multiplier multiplier() const
    {
        return m_multiplier;
    }

private:
    const Multiplier m_multiplier;
    AveragePerPeriod<Value> m_averageCalculator;
    const std::chrono::milliseconds m_period;
    ReportAnomalyFunc m_reportAnomalyFunc;

    bool m_hasEnoughData = false;
    std::optional<ElapsedTimer> m_initialDataAccumulationTimer;

    template<typename... AuxArgs>
    void testValue(Value value, AuxArgs&&... args)
    {
        auto average = m_averageCalculator.getAveragePerLastPeriod();
        if (average == Value()) // aka zero
            average = Value(1);
        const auto threshold = average * m_multiplier;
        const bool anomaly = m_multiplier >= 1
            ? value > threshold
            : value < threshold;

        if (anomaly)
            m_reportAnomalyFunc(value, average, std::forward<AuxArgs>(args)...);
    }
};

} // namespace nx::utils::math
