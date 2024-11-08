// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>
#include <vector>

#include <QtCore/QVector>

namespace nx::vms::common {

template<class Period, class PeriodList>
class TimePeriods
{
public:
    /** Merge some time period lists into one. */
    static PeriodList merge(
        const std::vector<PeriodList>& periodLists,
        int limit = std::numeric_limits<int>::max(),
        Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder)
    {
        QVector<PeriodList> nonEmptyPeriods;
        for (const PeriodList& periodList: periodLists)
        {
            if (!periodList.empty())
                nonEmptyPeriods << periodList;
        }

        if (nonEmptyPeriods.empty())
            return PeriodList();

        if (nonEmptyPeriods.size() == 1)
        {
            PeriodList result = nonEmptyPeriods.first();
            if ((int) result.size() > limit && limit > 0)
                result.resize(limit);
            return result;
        }

        return sortOrder == Qt::SortOrder::AscendingOrder
            ? mergeTimePeriodsAsc(nonEmptyPeriods, limit)
            : mergeTimePeriodsDesc(nonEmptyPeriods, limit);
    }

private:
    template <typename IteratorPeriod>
    static PeriodList mergeTimePeriodsInternal(
        const QVector<PeriodList>& nonEmptyPeriods,
        std::vector<IteratorPeriod>&& minIndices,
        std::vector<IteratorPeriod>&& maxIndices,
        int limit)
    {
        if (limit <= 0)
            limit = std::numeric_limits<int>::max();

        PeriodList result;

        int maxSize = std::min<int>(
            limit,
            std::max_element(
                nonEmptyPeriods.cbegin(),
                nonEmptyPeriods.cend(),
                [](const PeriodList& l, const PeriodList& r)
                {
                    return l.size() < r.size();
                })->size()
            );
        result.reserve(maxSize);

        int minIndex = 0;
        while (minIndex != -1)
        {
            auto minStartTime = std::chrono::milliseconds(Period::kMaxTimeValue);
            minIndex = -1;
            for (int i = 0; i < nonEmptyPeriods.size(); ++i)
            {
                const auto startIdx = minIndices[i];

                if (startIdx != maxIndices[i])
                {
                    const Period& startPeriod = *startIdx;
                    if (startPeriod.startTime() < minStartTime)
                    {
                        minIndex = i;
                        minStartTime = startPeriod.startTime();
                    }
                }
            }

            if (minIndex >= 0)
            {
                auto& startIdx = minIndices[minIndex];
                const Period& startPeriod = *startIdx;

                // Add chunk to merged data.
                if (result.empty())
                {
                    if ((int) result.size() >= limit)
                        return result;

                    result.push_back(startPeriod);
                    if (startPeriod.isInfinite())
                        return result;
                }
                else
                {
                    Period& last = result.back();
                    NX_ASSERT(
                        last.startTime() <= startPeriod.startTime(),
                        "Algorithm semantics failure, order failed");
                    if (startPeriod.isInfinite())
                    {
                        NX_ASSERT(!last.isInfinite(), "This should never happen");
                        if (last.isInfinite())
                        {
                            last.startTime() = qMin(last.startTime(), startPeriod.startTime());
                        }
                        else if (startPeriod.startTime() > last.startTime() + last.duration())
                        {
                            if ((int) result.size() >= limit)
                                return result;
                            result.push_back(startPeriod);
                        }
                        else
                        {
                            last.resetDuration();
                        }
                        // Last element is live and starts before all of rest - no need to process other elements.
                        return result;
                    }
                    else if (last.startTime() <= minStartTime
                        && last.startTime() + last.duration() >= minStartTime)
                    {
                        last.setDuration(qMax(
                            last.duration(),
                            minStartTime + startPeriod.duration() - last.startTime()));
                    }
                    else
                    {
                        if ((int) result.size() >= limit)
                            return result;

                        result.push_back(startPeriod);
                    }
                }
                startIdx++;
            }
        }
        return result;
    }

    static PeriodList mergeTimePeriodsAsc(const QVector<PeriodList>& nonEmptyPeriods, int limit)
    {
        std::vector<typename PeriodList::const_iterator> minIndices(nonEmptyPeriods.size());
        std::vector<typename PeriodList::const_iterator> endIndices(nonEmptyPeriods.size());
        for (int i = 0; i < nonEmptyPeriods.size(); ++i)
        {
            minIndices[i] = nonEmptyPeriods[i].cbegin();
            endIndices[i] = nonEmptyPeriods[i].cend();
        }
        return mergeTimePeriodsInternal<typename PeriodList::const_iterator>(
            nonEmptyPeriods,
            std::move(minIndices),
            std::move(endIndices),
            limit);
    }

    static PeriodList mergeTimePeriodsDesc(const QVector<PeriodList>& nonEmptyPeriods, int limit)
    {
        std::vector<typename PeriodList::const_reverse_iterator> minIndexes(nonEmptyPeriods.size());
        std::vector<typename PeriodList::const_reverse_iterator> endIndexes(nonEmptyPeriods.size());
        for (int i = 0; i < nonEmptyPeriods.size(); ++i)
        {
            minIndexes[i] = nonEmptyPeriods[i].crbegin();
            endIndexes[i] = nonEmptyPeriods[i].crend();
        }
        auto result = mergeTimePeriodsInternal<typename PeriodList::const_reverse_iterator>(
            nonEmptyPeriods,
            std::move(minIndexes),
            std::move(endIndexes),
            std::numeric_limits<int>::max());
        if (limit > 0 && (int) result.size() > limit)
            result.erase(result.begin(), result.begin() + (result.size() - limit));
        std::reverse(result.begin(), result.end());
        return result;
    }
};

} // namespace helpers
