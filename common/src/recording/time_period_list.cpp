#include "time_period_list.h"

#include <utils/common/util.h>
#include <utils/math/math.h>
#include <QElapsedTimer>

namespace {
    static const qint64 InvalidValue = INT64_MAX;
}

QnTimePeriodList::QnTimePeriodList(const QnTimePeriod &singlePeriod) :
    std::vector<QnTimePeriod>()
{
    push_back(singlePeriod);
}

QnTimePeriodList::const_iterator QnTimePeriodList::findNearestPeriod(qint64 timeMs, bool searchForward) const {
    if (isEmpty())
        return end();

    const_iterator itr = qUpperBound(cbegin(), cend(), timeMs);
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
    qint64 timeMs = timeUsec/1000;
    const_iterator period = findNearestPeriod(timeMs, searchForward);
    if (period != cend()) {
        if (period->contains(timeMs))
            return timeUsec;
        else if (searchForward)
            return period->startTimeMs*1000;
        else
            return period->endTimeMs()*1000;
    }
    else if (searchForward)
        return DATETIME_NOW;
    else 
        return 0;
}

bool QnTimePeriodList::intersects(const QnTimePeriod &period) const {        
    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if(lastPos != end())
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

bool QnTimePeriodList::containPeriod(const QnTimePeriod &period) const {
    auto found = std::find_if(cbegin(), cend(), [period](const QnTimePeriod &p){return p.contains(period);});
    return found != cend();
}

QnTimePeriodList QnTimePeriodList::intersected(const QnTimePeriod &period) const {
    QnTimePeriodList result;

    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if(lastPos != end())
        lastPos++;

    for (const_iterator pos = firstPos; pos != lastPos; ++pos) {
        QnTimePeriod p = pos->intersected(period);
        if(!p.isEmpty())
            result.push_back(p);
    }

    return result;
}

qint64 QnTimePeriodList::duration() const {
    if(isEmpty())
        return 0;

    if(back().isInfinite())
        return QnTimePeriod::infiniteDuration();

    qint64 result = 0;
    for(const QnTimePeriod &period: *this)
        result += period.durationMs;
    return result;
}


void saveField(QByteArray &stream, qint64 field, quint8 header, int dataLen)
{
    field = htonll(field << (64 - dataLen*8));
    quint8* data = (quint8*) &field;
    data[0] &= 0x3f;
    data[0] |= (header << 6);
    stream.append(((const char*) data), dataLen);
}
// field header:
// 00 - 14 bit to data
// 01 - 22 bit to data
// 10 - 30 bit to data
// 11 - 38 bit to data
// 11 and 0xffffffffff data - full 48 bit data
void serializeField(QByteArray &stream, qint64 field, bool intersected)
{
    if (intersected) {
        qint8 sign = (field < 0 ? -1 : 1);
        field *= sign;
        stream.append(((const char*) &sign), 1);
    }

    if (field < 0x4000ll)
        saveField(stream, field, 0, 2);
    else if (field < 0x400000ll)
        saveField(stream, field, 1, 3);
    else if (field < 0x40000000ll)
        saveField(stream, field, 2, 4);
    else if (field < 0x4000000000ll-1)
        saveField(stream, field, 3, 5);
    else {
        saveField(stream, 0xffffffffffll, 3, 5);
        qint64 data = htonll(field << 16);
        stream.append(((const char*) &data), 6);
    }
}

bool QnTimePeriodList::encode(QByteArray &stream, bool intersected)
{
    if (isEmpty())
        return true;

    qint64 timePos = first().startTimeMs;

    qint64 val = htonll(timePos << 16);
    stream.append(((const char*) &val), 6);

    bool first = true;
    for (const QnTimePeriod &period: *this) {
        qint64 timeDelta = period.startTimeMs - timePos;
        if (timeDelta < 0 && !intersected)
            return false;
        if (Q_LIKELY(!first)) {
            serializeField(stream, timeDelta, intersected);
            first = false;
        }
        serializeField(stream, period.durationMs+1, intersected);
        timePos += timeDelta + period.durationMs;
    }
    return true;
};

qint64 decodeValue(const quint8 *&curPtr, const quint8 *end)
{
    if (curPtr >= end)
        return InvalidValue;
    int fieldSize = 2 + (*curPtr >> 6);
    if (end-curPtr < fieldSize)
        return InvalidValue;
    qint64 value = *curPtr++ & 0x3f;
    for (int i = 0; i < fieldSize-1; ++i)
        value = (value << 8) + *curPtr++;
    if (value == 0x3fffffffffll) {
        if (end - curPtr < 6)
            return InvalidValue;
        value = *curPtr++;
        for (int i = 0; i < 5; ++i)
            value = (value << 8) + *curPtr++;
    }
    return value;
};

bool QnTimePeriodList::decode(const quint8 *data, int dataSize, bool intersected)
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
        qint8 sign = 1;
        if (intersected && curPtr < end)
            sign = *curPtr++;

        qint64 duration = decodeValue(curPtr, end);
        if (duration == InvalidValue) 
            return false;
        duration--;

        if (intersected)
            duration *= sign;

        fullStartTime += relStartTime;
        push_back(QnTimePeriod(fullStartTime, duration));
        fullStartTime += duration;

        if (intersected && curPtr < end)
            sign = *curPtr++;
        relStartTime = decodeValue(curPtr, end);

        if (intersected && relStartTime != InvalidValue)
            relStartTime *= sign;
    }
    return true;
}

bool QnTimePeriodList::decode(QByteArray &stream, bool intersected)
{
    return decode((const quint8 *) stream.constData(), stream.size(), intersected);
}



QnTimePeriodList QnTimePeriodList::aggregateTimePeriods(const QnTimePeriodList &periods, int detailLevelMs)
{
    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;

    result.push_back(*periods.begin());

    for (const QnTimePeriod &p: periods) {
        QnTimePeriod &last = result.last();
        if(last.isInfinite())
            break;

        if (last.startTimeMs + last.durationMs + detailLevelMs > p.startTimeMs) {
            if(p.isInfinite()) {
                last.durationMs = QnTimePeriod::infiniteDuration();
            } else {
                last.durationMs = qMax(last.durationMs, p.startTimeMs + p.durationMs - last.startTimeMs);
            }
        } else {
            result.push_back(p);
        }
    }

    return result;
}

QnTimePeriodList QnTimePeriodList::mergeTimePeriods(const QVector<QnTimePeriodList>& periods)
{
    if(periods.size() == 1)
        return periods.first();

    QVector<int> minIndexes(periods.size());
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1) {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        int i = 0;
        for (const QnTimePeriodList &periodsList: periods) {
            size_t startIdx = minIndexes[i];
            const QnTimePeriod &startPeriod = periodsList[startIdx];

            if (startIdx < periodsList.size() && startPeriod.startTimeMs < minStartTime) {
                minIndex = i;
                minStartTime = startPeriod.startTimeMs;
            }
            ++i;
        }

        if (minIndex >= 0) {
            int &startIdx = minIndexes[minIndex];
            const QnTimePeriodList &periodsList = periods[minIndex];
            const QnTimePeriod &startPeriod = periodsList[startIdx];

            // add chunk to merged data
            if (result.empty()) {
                result.push_back(startPeriod);
            } else {
                QnTimePeriod &last = result.last();
                if (startPeriod.isInfinite()) {
                    if (last.isInfinite())
                        last.startTimeMs = qMin(last.startTimeMs, startPeriod.startTimeMs);
                    else if (startPeriod.startTimeMs > last.startTimeMs+last.durationMs)
                        result.push_back(startPeriod);
                    else 
                        last.durationMs = QnTimePeriod::infiniteDuration();
                    break;
                } else if (last.startTimeMs <= minStartTime && last.startTimeMs+last.durationMs >= minStartTime) {
                    last.durationMs = qMax(last.durationMs, minStartTime + startPeriod.durationMs - last.startTimeMs);
                } else {
                    result.push_back(startPeriod);
                }
            } 
            startIdx++;
        }
    }
    return result;
}
