#include "time_period_list.h"

#include <utils/common/util.h>
#include <utils/math/math.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/network/socket_common.h>
#include <nx/utils/datetime.h>

QN_FUSION_ADAPT_STRUCT(MultiServerPeriodData, (guid)(periods))
QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((MultiServerPeriodData), (json)(ubjson)(xml)(csv_record)(compressed_time)(eq))

namespace {
static const qint64 InvalidValue = std::numeric_limits<qint64>::max();
}

QnTimePeriodList::QnTimePeriodList(const QnTimePeriod &singlePeriod):
    base_type()
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

bool QnTimePeriodList::intersects(const QnTimePeriod &period) const
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

QnTimePeriodList QnTimePeriodList::intersected(const QnTimePeriod &period) const
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
        return QnTimePeriod::infiniteDuration();

    qint64 result = 0;
    for (const QnTimePeriod &period : *this)
        result += period.durationMs;
    return result;
}

QnTimePeriod QnTimePeriodList::boundingPeriod(qint64 truncateInfinite /* = QnTimePeriod::infiniteDuration()*/) const
{
    if (isEmpty())
        return QnTimePeriod();

    QnTimePeriod result(first());
    if (last().isInfinite())
    {
        if (truncateInfinite == QnTimePeriod::infiniteDuration())
            result.durationMs = QnTimePeriod::infiniteDuration();
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

QnTimePeriodList QnTimePeriodList::aggregateTimePeriods(const QnTimePeriodList &periods, int detailLevelMs)
{
    /* Do not aggregate periods by 1 ms */
    if (detailLevelMs <= 1)
        return periods;

    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;

    result.push_back(*periods.begin());

    for (const QnTimePeriod &p : periods)
    {
        QnTimePeriod &last = result.last();
        if (last.isInfinite())
            break;

        if (last.startTimeMs + last.durationMs + detailLevelMs > p.startTimeMs)
        {
            if (p.isInfinite())
            {
                last.durationMs = QnTimePeriod::infiniteDuration();
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
    auto srcItr = cbegin();
    auto subtrahendItr = periodList.cbegin();
    QnTimePeriodList result;

    while (srcItr != end())
    {
        if (srcItr->isContainedIn(*subtrahendItr))
        {
            ++srcItr;
            continue;
        }

        if (subtrahendItr != periodList.cend()
            && subtrahendItr->endTimeMs() <= srcItr->startTimeMs)
        {
            ++subtrahendItr;
            continue;
        }

        if (subtrahendItr == periodList.cend() && srcItr != end())
            result << *srcItr;

        auto current = *srcItr;
        while (subtrahendItr != periodList.cend())
        {
            if (current.isLeftIntersection(*subtrahendItr))
            {
                current = current.truncatedFront(subtrahendItr->endTimeMs());
                ++subtrahendItr;

                if (subtrahendItr == periodList.cend())
                    result << current;

                continue;
            }

            if (current.isRightIntersection(*subtrahendItr))
            {
                result << current.truncated(subtrahendItr->startTimeMs);
                break;
            }

            if (current.contains(*subtrahendItr))
            {
                result << QnTimePeriod::fromInterval(
                    current.startTimeMs,
                    subtrahendItr->startTimeMs);

                current = QnTimePeriod::fromInterval(
                    subtrahendItr->endTimeMs(),
                    current.endTimeMs());

                ++subtrahendItr;

                if (subtrahendItr == periodList.cend())
                    result << current;

                continue;
            }

            result << current;
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

        NX_ASSERT(eraseIter != periods.begin(), Q_FUNC_INFO, "Invalid semantics");

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

        while (eraseIter != periods.end())
            eraseIter = periods.erase(eraseIter);
    }


    //NX_ASSERT(!periods.isEmpty(), Q_FUNC_INFO, "Empty list should be worked out earlier");
    if (periods.isEmpty())
    {
        periods = tail;
        return;
    }

    auto last = periods.end() - 1;
    if (!tail.isEmpty() && tail.first().startTimeMs < last->endTimeMs())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Should not get here, security fallback");
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


QnTimePeriodList QnTimePeriodList::mergeTimePeriods(const std::vector<QnTimePeriodList>& periodLists, int limit)
{
    QVector<QnTimePeriodList> nonEmptyPeriods;
    for (const QnTimePeriodList &periodList : periodLists)
        if (!periodList.isEmpty())
            nonEmptyPeriods << periodList;

    if (nonEmptyPeriods.empty())
        return QnTimePeriodList();

    if (nonEmptyPeriods.size() == 1)
    {
        QnTimePeriodList result = nonEmptyPeriods.first();
        if (result.size() > limit)
            result.resize(limit);
        return result;
    }

    std::vector< QnTimePeriodList::const_iterator > minIndices(nonEmptyPeriods.size());
    for (int i = 0; i < nonEmptyPeriods.size(); ++i)
        minIndices[i] = nonEmptyPeriods[i].cbegin();

    QnTimePeriodList result;

    int maxSize = std::min<int>(
        limit,
        std::max_element(nonEmptyPeriods.cbegin(), nonEmptyPeriods.cend(), [](const QnTimePeriodList &l, const QnTimePeriodList &r) { return l.size() < r.size(); })->size()
        );
    result.reserve(maxSize);

    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        int i = 0;
        for (const QnTimePeriodList &periodsList : nonEmptyPeriods)
        {
            const auto startIdx = minIndices[i];

            if (startIdx != periodsList.cend())
            {
                const QnTimePeriod &startPeriod = *startIdx;
                if (startPeriod.startTimeMs < minStartTime)
                {
                    minIndex = i;
                    minStartTime = startPeriod.startTimeMs;
                }
            }
            ++i;
        }

        if (minIndex >= 0)
        {
            auto &startIdx = minIndices[minIndex];
            //const QnTimePeriodList &periodsList = nonEmptyPeriods[minIndex];
            const QnTimePeriod &startPeriod = *startIdx;

            // add chunk to merged data
            if (result.empty())
            {
                if (result.size() >= limit)
                    return result;
                result.push_back(startPeriod);
                if (startPeriod.isInfinite())
                    return result;
            }
            else
            {
                QnTimePeriod &last = result.last();
                NX_ASSERT(last.startTimeMs <= startPeriod.startTimeMs, Q_FUNC_INFO, "Algorithm semantics failure, order failed");
                if (startPeriod.isInfinite())
                {
                    NX_ASSERT(!last.isInfinite(), Q_FUNC_INFO, "This should never happen");
                    if (last.isInfinite())
                        last.startTimeMs = qMin(last.startTimeMs, startPeriod.startTimeMs);
                    else if (startPeriod.startTimeMs > last.startTimeMs + last.durationMs)
                    {
                        if (result.size() >= limit)
                            return result;
                        result.push_back(startPeriod);
                    }
                    else
                        last.durationMs = QnTimePeriod::infiniteDuration();
                    /* Last element is live and starts before all of rest - no need to process other elements. */
                    return result;
                }
                else if (last.startTimeMs <= minStartTime && last.startTimeMs + last.durationMs >= minStartTime)
                {
                    last.durationMs = qMax(last.durationMs, minStartTime + startPeriod.durationMs - last.startTimeMs);
                }
                else
                {
                    if (result.size() >= limit)
                        return result;
                    result.push_back(startPeriod);
                }
            }
            startIdx++;
        }
    }
    return result;
}

QnTimePeriodList QnTimePeriodList::simplified() const
{
    QnTimePeriodList newList;

    for (const QnTimePeriod &period : *this)
        newList.includeTimePeriod(period);

    return newList;
}
