#include "time_period.h"

#include <QtCore/QDebug>

#include <utils/common/util.h>

#include "time_period_list.h"


bool operator < (const QnTimePeriod& first, const QnTimePeriod& other) 
{
    return first.startTimeMs < other.startTimeMs;
}

bool operator < (qint64 first, const QnTimePeriod& other) 
{ 
    return first < other.startTimeMs; 
}

bool operator < (const QnTimePeriod& other, qint64 first) 
{ 
    return other.startTimeMs < first;
}

void QnTimePeriod::clear()
{
    startTimeMs = 0;
    durationMs = 0;
}

qint64 QnTimePeriod::endTimeMs() const
{
    if (durationMs == -1)
        return DATETIME_NOW;
    else
        return startTimeMs + durationMs;
}

bool QnTimePeriod::contains(const QnTimePeriod &timePeriod) const
{
    return startTimeMs <= timePeriod.startTimeMs && (startTimeMs + durationMs >= timePeriod.startTimeMs + timePeriod.durationMs);
}

bool QnTimePeriod::contains(qint64 timeMs) const
{
    return qBetween(timeMs, startTimeMs, durationMs != -1 ? startTimeMs + durationMs : DATETIME_NOW);
}

void QnTimePeriod::addPeriod(const QnTimePeriod &timePeriod)
{
    qint64 endPoint1 = startTimeMs + durationMs;
    qint64 endPoint2 = timePeriod.startTimeMs + timePeriod.durationMs;

    startTimeMs = qMin(startTimeMs, timePeriod.startTimeMs);
    if (durationMs == -1 || timePeriod.durationMs == -1)
        durationMs = -1;
    else
        durationMs = qMax(endPoint1, endPoint2) - startTimeMs;
}

QnTimePeriod QnTimePeriod::intersected(const QnTimePeriod &other) const
{
    if (durationMs == -1 && other.durationMs == -1)
        return QnTimePeriod(qMax(startTimeMs, other.startTimeMs), -1);

    if (durationMs == -1){
        if (startTimeMs > other.startTimeMs + other.durationMs)
            return QnTimePeriod();
        if (startTimeMs < other.startTimeMs)
            return QnTimePeriod(other.startTimeMs, other.durationMs);
        return QnTimePeriod(startTimeMs, other.durationMs - (startTimeMs - other.startTimeMs));
    }

    if (other.durationMs == -1){
        if (other.startTimeMs > startTimeMs + durationMs)
            return QnTimePeriod();
        if (other.startTimeMs < startTimeMs)
            return QnTimePeriod(startTimeMs, durationMs);
        return QnTimePeriod(other.startTimeMs, durationMs - (other.startTimeMs - startTimeMs));
    }

    if (other.startTimeMs > startTimeMs + durationMs || startTimeMs > other.startTimeMs + other.durationMs)
        return QnTimePeriod();

    qint64 start = qMax(startTimeMs, other.startTimeMs);
    qint64 end = qMin(startTimeMs + durationMs, other.startTimeMs + other.durationMs);
    return QnTimePeriod(start, end - start);
}

QnTimePeriodList QnTimePeriod::aggregateTimePeriods(const QnTimePeriodList &periods, int detailLevelMs)
{
    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;
    result << periods[0];

    for (int i = 1; i < periods.size(); ++i)
    {
        QnTimePeriod& last = result.last();
        if(last.durationMs == -1)
            break;

        if (last.startTimeMs + last.durationMs + detailLevelMs > periods[i].startTimeMs) {
            if(periods[i].durationMs == -1) {
                last.durationMs = -1;
            } else {
                last.durationMs = qMax(last.durationMs, periods[i].startTimeMs + periods[i].durationMs - last.startTimeMs);
            }
        } else {
            result << periods[i];
        }
    }

    return result;
}

QnTimePeriodList QnTimePeriod::mergeTimePeriods(const QVector<QnTimePeriodList>& periods)
{
    QVector<int> minIndexes;
    minIndexes.resize(periods.size());
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        for (int i = 0; i < periods.size(); ++i) 
        {
            int startIdx = minIndexes[i];
            if (startIdx < periods[i].size() && periods[i][startIdx].startTimeMs < minStartTime) {
                minIndex = i;
                minStartTime = periods[i][startIdx].startTimeMs;
            }
        }

        if (minIndex >= 0)
        {
            int& startIdx = minIndexes[minIndex];
            // add chunk to merged data
            if (result.isEmpty()) {
                result << periods[minIndex][startIdx];
            }
            else {
                QnTimePeriod& last = result.last();
                if (periods[minIndex][startIdx].durationMs == -1) 
                {
                    if (last.durationMs == -1)
                        last.startTimeMs = qMin(last.startTimeMs, periods[minIndex][startIdx].startTimeMs);
                    else if (periods[minIndex][startIdx].startTimeMs > last.startTimeMs+last.durationMs)
                        result << periods[minIndex][startIdx];
                    else 
                        last.durationMs = -1;
                    break;
                }
                else if (last.startTimeMs <= minStartTime && last.startTimeMs+last.durationMs >= minStartTime)
                    last.durationMs = qMax(last.durationMs, minStartTime + periods[minIndex][startIdx].durationMs - last.startTimeMs);
                else {
                    result << periods[minIndex][startIdx];
                }
            } 
            startIdx++;
        }
    }
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
void serializeField(QByteArray &stream, qint64 field)
{
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

bool QnTimePeriod::encode(QByteArray &stream, const QnTimePeriodList &periods)
{
    if (periods.isEmpty())
        return true;
    qint64 timePos = periods.first().startTimeMs;

    qint64 val = htonll(timePos << 16);
    stream.append(((const char*) &val), 6);

    for (int i = 0; i < periods.size(); ++i)
    {
        qint64 timeDelta = periods[i].startTimeMs - timePos;
        if (timeDelta < 0)
            return false;
        if (i > 0)
            serializeField(stream, timeDelta);
        serializeField(stream, periods[i].durationMs+1);
        timePos += timeDelta + periods[i].durationMs;
    }
    return true;
};

qint64 decodeValue(const quint8 *&curPtr, const quint8 *end)
{
    if (curPtr >= end)
        return -1;
    int fieldSize = 2 + (*curPtr >> 6);
    if (end-curPtr < fieldSize)
        return -1;
    qint64 value = *curPtr++ & 0x3f;
    for (int i = 0; i < fieldSize-1; ++i)
        value = (value << 8) + *curPtr++;
    if (value == 0x3fffffffffll) {
        if (end - curPtr < 6)
            return -1;
        value = *curPtr++;
        for (int i = 0; i < 5; ++i)
            value = (value << 8) + *curPtr++;
    }
    return value;
};

bool QnTimePeriod::decode(const quint8 *data, int dataSize, QnTimePeriodList &periods)
{
    periods.clear();

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
    while (relStartTime != -1)
    {
        qint64 duration = decodeValue(curPtr, end);
        if (duration == -1) 
            return false;
        duration--;
        fullStartTime += relStartTime;
        periods << QnTimePeriod(fullStartTime, duration);
        fullStartTime += duration;
        relStartTime = decodeValue(curPtr, end);
    }
    return true;
}

bool QnTimePeriod::decode(QByteArray &stream, QnTimePeriodList &periods)
{
    return decode((const quint8 *) stream.constData(), stream.size(), periods);
}

bool QnTimePeriod::operator==(const QnTimePeriod &other) const
{
    return startTimeMs == other.startTimeMs && durationMs == other.durationMs;
}

QDebug operator<<(QDebug dbg, const QnTimePeriod &period) {
    dbg.nospace() << "QnTimePeriod(" << period.startTimeMs << ',' << period.durationMs << ')';
    return dbg.space();
}
