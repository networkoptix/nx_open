// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_period_list.h"

#include <utils/common/util.h>
#include <utils/math/math.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/network/socket_common.h> //< For htonll().
#include <nx/utils/datetime.h>
#include <nx/vms/common/recording/time_periods_utils.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MultiServerPeriodData,
    (json)(ubjson)(xml)(csv_record)(compressed_time), (guid)(periods))

namespace {

static const qint64 InvalidValue = std::numeric_limits<qint64>::max();

} // namespace

QnTimePeriodList::QnTimePeriodList(const QnTimePeriod &singlePeriod):
    QnTimePeriodList()
{
    push_back(singlePeriod);
}

QnTimePeriodList::QnTimePeriodList():
    base_type()
{
}

QnTimePeriodList::const_iterator QnTimePeriodList::findNearestPeriod(qint64 timeMs, bool searchForward) const
{
    if (isEmpty())
        return end();

    const_iterator itr = std::upper_bound(cbegin(), cend(), timeMs);
    if (itr != begin())
        --itr;

    /* Note that there is no need to check for itr != end() here as
     * the container is not empty. */
    if (searchForward && itr->endTimeMs() <= timeMs)
        ++itr;

    return itr;
}

qint64 QnTimePeriodList::roundTimeToPeriodUSec(qint64 timeUsec, bool searchForward) const
{
    qint64 timeMs = timeUsec / 1000;
    const_iterator period = findNearestPeriod(timeMs, searchForward);
    if (period != cend())
    {
        if (period->contains(timeMs))
            return timeUsec;
        else if (searchForward)
            return period->startTimeMs * 1000;
        else
            return period->endTimeMs() * 1000;
    }
    else if (searchForward)
        return DATETIME_NOW;
    else
        return 0;
}

bool QnTimePeriodList::intersects(const QnTimePeriod& period) const
{
    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if (lastPos != end())
        lastPos++;

    for (const_iterator pos = firstPos; pos != lastPos; ++pos)
        if (!pos->intersected(period).isEmpty())
            return true;
    return false;
}

bool QnTimePeriodList::containTime(qint64 timeMs) const
{
    const_iterator firstPos = findNearestPeriod(timeMs, true);
    if (firstPos != end())
        return firstPos->contains(timeMs);
    else
        return false;
}

bool QnTimePeriodList::containPeriod(const QnTimePeriod &period) const
{
    auto found = std::find_if(cbegin(), cend(), [period](const QnTimePeriod &p) { return p.contains(period); });
    return found != cend();
}

QnTimePeriodList QnTimePeriodList::intersected(const QnTimePeriod& period) const
{
    QnTimePeriodList result;

    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if (lastPos != end())
        lastPos++;

    for (const_iterator pos = firstPos; pos != lastPos; ++pos)
    {
        QnTimePeriod p = pos->intersected(period);
        if (!p.isEmpty())
            result.push_back(p);
    }

    return result;
}

QnTimePeriodList QnTimePeriodList::intersected(const QnTimePeriodList& other) const
{
    QnTimePeriodList result;
    const auto list1 = simplified();
    const auto list2 = other.simplified();
    auto first = list1.begin();
    auto second = list2.begin();
    for (; first != list1.end() && second != list2.end();)
    {
        const auto intersection = first->intersected(*second);
        if (intersection.isNull())
        {
            if (*first < *second)
                ++first;
            else
                ++second;
            continue;
        }

        result.push_back(intersection);
        if (first->endTime() < second->endTime())
            ++first;
        else if (second->endTime() < first->endTime())
            ++second;
        else
        {
            ++first;
            ++second;
        }
    }

    return result;
}

QnTimePeriodList QnTimePeriodList::intersectedPeriods(const QnTimePeriod &period) const
{
    QnTimePeriodList result;
    if (isEmpty())
        return result;

    qint64 endTimeMs = period.endTimeMs();
    auto iter = findNearestPeriod(period.startTimeMs, true);

    while (iter != cend() && iter->startTimeMs < endTimeMs)
    {
        result.push_back(*iter);
        ++iter;
    }

    return result;
}


qint64 QnTimePeriodList::duration() const
{
    if (isEmpty())
        return 0;

    if (back().isInfinite())
        return QnTimePeriod::kInfiniteDuration;

    qint64 result = 0;
    for (const QnTimePeriod &period : *this)
        result += period.durationMs;
    return result;
}

QnTimePeriod QnTimePeriodList::boundingPeriod(qint64 truncateInfinite /* = QnTimePeriod::kInfiniteDuration*/) const
{
    if (isEmpty())
        return QnTimePeriod();

    QnTimePeriod result(first());
    if (last().isInfinite())
    {
        if (truncateInfinite == QnTimePeriod::kInfiniteDuration)
            result.durationMs = QnTimePeriod::kInfiniteDuration;
        else
        {
            result.durationMs = std::max(0ll, truncateInfinite - result.startTimeMs);
        }
    }
    else
    {
        result.durationMs = std::max(0ll, last().endTimeMs() - result.startTimeMs);
    }
    if (!result.isValid())
        return QnTimePeriod();
    return result;
}

void saveField(QByteArray &stream, qint64 field, quint8 header, int dataLen)
{
    field = htonll(field << (64 - dataLen * 8));
    quint8* data = (quint8*)&field;
    data[0] &= 0x3f;
    data[0] |= (header << 6);
    stream.append(((const char*)data), dataLen);
}
// field header:
// 00 - 14 bit to data
// 01 - 22 bit to data
// 10 - 30 bit to data
// 11 - 38 bit to data
// 11 and 0xffffffffff data - full 48 bit data
void serializeField(QByteArray &stream, qint64 field)
{
    if (field < 0x4000ll)
        saveField(stream, field, 0, 2);
    else if (field < 0x400000ll)
        saveField(stream, field, 1, 3);
    else if (field < 0x40000000ll)
        saveField(stream, field, 2, 4);
    else if (field < 0x4000000000ll - 1)
        saveField(stream, field, 3, 5);
    else
    {
        saveField(stream, 0xffffffffffll, 3, 5);
        qint64 data = htonll(field << 16);
        stream.append(((const char*)&data), 6);
    }
}

bool QnTimePeriodList::encode(QByteArray &stream)
{
    if (isEmpty())
        return true;

    qint64 timePos = first().startTimeMs;

    qint64 val = htonll(timePos << 16);
    stream.append(((const char*)&val), 6);

    bool first = true;
    for (const QnTimePeriod &period : *this)
    {
        qint64 timeDelta = period.startTimeMs - timePos;
        if (timeDelta < 0)
            return false;
        if (Q_LIKELY(!first))
            serializeField(stream, timeDelta);
        else
            first = false;
        serializeField(stream, period.durationMs + 1);
        timePos += timeDelta + period.durationMs;
    }
    return true;
};

qint64 decodeValue(const quint8 *&curPtr, const quint8 *end)
{
    if (curPtr >= end)
        return InvalidValue;
    int fieldSize = 2 + (*curPtr >> 6);
    if (end - curPtr < fieldSize)
        return InvalidValue;
    qint64 value = *curPtr++ & 0x3f;
    for (int i = 0; i < fieldSize - 1; ++i)
        value = (value << 8) + *curPtr++;
    if (value == 0x3fffffffffll)
    {
        if (end - curPtr < 6)
            return InvalidValue;
        value = *curPtr++;
        for (int i = 0; i < 5; ++i)
            value = (value << 8) + *curPtr++;
    }
    return value;
};

bool QnTimePeriodList::decode(const quint8 *data, int dataSize)
{
    clear();

    const quint8* curPtr = data;
    const quint8* end = data + dataSize;
    if (end - curPtr < 6)
        return false;

    qint64 fullStartTime = 0;
    memcpy(&fullStartTime, curPtr, 6);
    curPtr += 6;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    fullStartTime <<= 16;
#else
    fullStartTime >>= 16;
#endif
    fullStartTime = ntohll(fullStartTime);

    qint64 relStartTime = 0;
    while (relStartTime != InvalidValue)
    {
        qint64 duration = decodeValue(curPtr, end);
        if (duration == InvalidValue)
            return false;
        duration--;

        fullStartTime += relStartTime;
        push_back(QnTimePeriod(fullStartTime, duration));
        fullStartTime += duration;

        relStartTime = decodeValue(curPtr, end);
    }
    return true;
}

bool QnTimePeriodList::decode(QByteArray &stream)
{
    return decode((const quint8 *)stream.constData(), stream.size());
}

QnTimePeriodList QnTimePeriodList::aggregateTimePeriodsUnconstrained(
    const QnTimePeriodList& periods,
    std::chrono::milliseconds detailLevel)
{
    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;

    result.push_back(*periods.begin());

    for (const QnTimePeriod& p: periods)
    {
        QnTimePeriod &last = result.last();
        if (last.isInfinite())
            break;

        if (last.startTimeMs + last.durationMs + detailLevel.count() > p.startTimeMs)
        {
            if (p.isInfinite())
            {
                last.durationMs = QnTimePeriod::kInfiniteDuration;
            }
            else
            {
                last.durationMs = qMax(last.durationMs, p.startTimeMs + p.durationMs - last.startTimeMs);
            }
        }
        else
        {
            result.push_back(p);
        }
    }

    return result;
}

QnTimePeriodList QnTimePeriodList::aggregateTimePeriods(
    const QnTimePeriodList& periods,
    int detailLevelMs)
{
    return aggregateTimePeriods(periods, std::chrono::milliseconds(detailLevelMs));
}

QnTimePeriodList QnTimePeriodList::aggregateTimePeriods(
    const QnTimePeriodList& periods,
    std::chrono::milliseconds detailLevel)
{
    // Do not aggregate periods by 1 ms.
    if (detailLevel <= std::chrono::milliseconds(1))
        return periods;

    return aggregateTimePeriodsUnconstrained(periods, detailLevel);
}

QnTimePeriodList QnTimePeriodList::intersection(
    const QnTimePeriodList& first, const QnTimePeriodList& second)
{
    QnTimePeriodList result;
    auto firstIt = first.begin();
    auto secondIt = second.begin();
    while (true)
    {
        if (firstIt == first.end() || secondIt == second.end())
            return result;

        const auto intersected = firstIt->intersected(*secondIt);
        if (intersected.isEmpty())
        {
            if (firstIt->startTime() < secondIt->startTime())
                ++firstIt;
            else
                ++secondIt;
        }
        else
        {
            result.push_back(intersected);
            if (firstIt->endTime() < secondIt->endTime())
                ++firstIt;
            else
                ++secondIt;
        }
    }

    return result;
}

void QnTimePeriodList::includeTimePeriod(const QnTimePeriod &period)
{
    if (period.isEmpty())
        return;

    if (empty())
    {
        push_back(period);
        return;
    }

    auto startIt = std::lower_bound(begin(), end(), period.startTimeMs);
    if (startIt != begin() && period.startTimeMs <= (startIt - 1)->endTimeMs())
        --startIt;

    qint64 periodEndMs = period.endTimeMs();

    auto endIt = startIt;
    while (endIt != end() && endIt->startTimeMs <= periodEndMs)
        ++endIt;

    if (startIt == endIt)
    {
        insert(startIt, period);
        return;
    }

    periodEndMs = std::max(periodEndMs, startIt->endTimeMs());
    startIt->startTimeMs = std::min(period.startTimeMs, startIt->startTimeMs);

    auto it = startIt + 1;
    for (int count = std::distance(it, endIt); count > 0; --count)
    {
        qint64 endMs = it->endTimeMs();
        if (periodEndMs < endMs)
            periodEndMs = endMs;

        it = erase(it);
    }

    startIt->durationMs = periodEndMs - startIt->startTimeMs;
}

void QnTimePeriodList::excludeTimePeriod(const QnTimePeriod &period)
{
    if (empty() || period.isEmpty())
        return;

    qint64 endTimeMs = period.endTimeMs();

    auto it = std::lower_bound(begin(), end(), period.startTimeMs);
    if (it != begin() && period.startTimeMs <= (it - 1)->endTimeMs())
        --it;

    while (it != end() && it->startTimeMs < endTimeMs)
    {
        bool contains = true;

        if (it->startTimeMs < period.startTimeMs)
        {
            it->durationMs = period.startTimeMs - it->startTimeMs;
            contains = false;
        }

        qint64 currentEndMs = it->endTimeMs();
        if (currentEndMs > endTimeMs)
        {
            it->durationMs = currentEndMs - endTimeMs;
            it->startTimeMs = endTimeMs;
            contains = false;
        }

        if (contains)
            it = erase(it);
        else
            ++it;
    }
}

void QnTimePeriodList::excludeTimePeriods(const QnTimePeriodList& periodList)
{
    auto srcItr = begin();
    auto subtrahendItr = periodList.cbegin();
    QnTimePeriodList result;

    while (srcItr != end())
    {
        if (subtrahendItr == periodList.cend())
        {
            result.push_back(*srcItr);
            ++srcItr;
            continue;
        }

        if (srcItr->isContainedIn(*subtrahendItr))
        {
            ++srcItr;
            continue;
        }

        if (subtrahendItr->endTimeMs() <= srcItr->startTimeMs)
        {
            ++subtrahendItr;
            continue;
        }

        auto current = *srcItr;
        while (subtrahendItr != periodList.cend())
        {
            if (current.isLeftIntersection(*subtrahendItr))
            {
                current = current.truncatedFront(subtrahendItr->endTimeMs());
                ++subtrahendItr;

                if (subtrahendItr == periodList.cend())
                    result.push_back(current);

                continue;
            }

            if (current.isRightIntersection(*subtrahendItr))
            {
                result.push_back(current.truncated(subtrahendItr->startTimeMs));
                break;
            }

            if (current.contains(*subtrahendItr))
            {
                result.push_back(QnTimePeriod::fromInterval(
                    current.startTimeMs,
                    subtrahendItr->startTimeMs));

                current = QnTimePeriod::fromInterval(
                    subtrahendItr->endTimeMs(),
                    current.endTimeMs());

                ++subtrahendItr;

                if (subtrahendItr == periodList.cend())
                    result.push_back(current);

                continue;
            }

            result.push_back(current);
            break;
        }

        // We should be here only if current source chunk is fully processed.
        ++srcItr;
    }

    *this = result;
}

void QnTimePeriodList::overwriteTail(QnTimePeriodList& periods, const QnTimePeriodList& tail, qint64 dividerPoint)
{
    qint64 erasePoint = dividerPoint;
    if (!tail.isEmpty())
        erasePoint = std::min(dividerPoint, tail.cbegin()->startTimeMs);

    if (periods.empty() || erasePoint <= periods.cbegin()->startTimeMs)
    {
        periods = tail;
        return;
    }

    if (erasePoint < DATETIME_NOW)
    {
        auto eraseIter = std::lower_bound(periods.begin(), periods.end(), erasePoint);

        NX_ASSERT(eraseIter != periods.begin(), "Invalid semantics");

        if (eraseIter != periods.begin() &&
            (eraseIter == periods.end() || eraseIter->startTimeMs > erasePoint)
            )
        {
            eraseIter--; /* We are sure, list is not empty. */
            if (eraseIter->isInfinite() || eraseIter->endTimeMs() > erasePoint)
                eraseIter->durationMs = std::max(0ll, erasePoint - eraseIter->startTimeMs);
            if (!eraseIter->isValid())
                eraseIter = periods.erase(eraseIter);
            else
                eraseIter++;
        }

        if (eraseIter != periods.end())
            periods.erase(eraseIter, periods.end());
    }


    //NX_ASSERT(!periods.isEmpty(), "Empty list should be worked out earlier");
    if (periods.isEmpty())
    {
        periods = tail;
        return;
    }

    auto last = periods.end() - 1;
    if (!tail.isEmpty() && tail.first().startTimeMs < last->endTimeMs())
    {
        NX_ASSERT(false, "Should not get here, security fallback");
        unionTimePeriods(periods, tail);
    }
    else if (!tail.isEmpty())
    {
        auto appending = tail.cbegin();
        /* Combine chunks if new chunk starts when the previous ends. */
        if (last->endTimeMs() == appending->startTimeMs)
        {
            last->addPeriod(*appending);
            ++appending;
        }
        while (appending != tail.cend())
        {
            periods.push_back(*appending);
            ++appending;
        }
    }
}

void QnTimePeriodList::unionTimePeriods(QnTimePeriodList& basePeriods, const QnTimePeriodList &appendingPeriods)
{
    if (appendingPeriods.isEmpty())
        return;

    if (basePeriods.isEmpty())
    {
        basePeriods = appendingPeriods;
        return;
    }
    basePeriods.reserve(basePeriods.size() + appendingPeriods.size());

    iterator iter = basePeriods.begin();
    for (const QnTimePeriod& period : appendingPeriods)
    {
        iter = std::upper_bound(iter, basePeriods.end(), period.startTimeMs);
        if (iter != basePeriods.begin())
            --iter;

        /* Note that there is no need to check for itr != end() here as the container is not empty. */
        if (iter->endTimeMs() < period.startTimeMs)
        {
            /* Space is empty, just inserting period or prepending to next. */
            ++iter;
            if (iter != basePeriods.end() && iter->startTimeMs <= period.endTimeMs())
                iter->addPeriod(period);
            else
                iter = basePeriods.insert(iter, period);
        }
        else if (iter->startTimeMs > period.endTimeMs())
        {
            /* Prepending before the first chunk. */
            iter = basePeriods.insert(iter, period);
        }
        else
        {
            /* Periods are overlapped. */
            iter->addPeriod(period);

            /* Try to combine with the following */
            auto next = iter + 1;
            while (next != basePeriods.end() && next->startTimeMs <= iter->endTimeMs())
            {
                iter->addPeriod(*next);
                next = basePeriods.erase(next);
                iter = next - 1;
            }
        }
    }
}

QnTimePeriodList QnTimePeriodList::mergeTimePeriods(
    const std::vector<QnTimePeriodList>& periodLists,
    int limit,
    Qt::SortOrder sortOrder)
{
    return nx::vms::common::TimePeriods<QnTimePeriod, QnTimePeriodList>::merge(
        periodLists,
        limit,
        sortOrder
    );
}

QnTimePeriodList QnTimePeriodList::simplified() const
{
    QnTimePeriodList newList;

    for (const QnTimePeriod &period : *this)
        newList.includeTimePeriod(period);

    return newList;
}
