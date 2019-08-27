#pragma once

#include <chrono>
#include <functional>
#include <optional>

#include "average_per_period.h"

namespace nx::utils::math {

/**
 * Calcultes elapsed time using std::chrono::steady_clock.
 * @param Duration std::chrono duration type.
 */
template<typename Duration = std::chrono::milliseconds>
class ElapsedTimer
{
public:
    ElapsedTimer():
        m_start(std::chrono::steady_clock::now())
    {
    }

    Duration elapsed() const
    {
        return std::chrono::duration_cast<Duration>(
            std::chrono::steady_clock::now() - m_start);
    }

    /**
     * @return Elapsed time.
     */
    Duration restart()
    {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<Duration>(
            std::chrono::steady_clock::now() - std::exchange(m_start, now));
    }

private:
    std::chrono::steady_clock::time_point m_start;
};

//-------------------------------------------------------------------------------------------------

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
                m_initialDataAccumulationTimer = ElapsedTimer<>();

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
    std::optional<ElapsedTimer<>> m_initialDataAccumulationTimer;

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
