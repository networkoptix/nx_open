#include "time_period.h"
#include "utils/common/util.h"

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

bool QnTimePeriod::containPeriod(const QnTimePeriod& timePeriod) const
{
    return startTimeMs <= timePeriod.startTimeMs && (startTimeMs + durationMs >= timePeriod.startTimeMs + timePeriod.durationMs);
}

bool QnTimePeriod::containTime(qint64 timeMs) const
{
    return qnBetween(timeMs, startTimeMs, startTimeMs+durationMs);
}


void QnTimePeriod::addPeriod(const QnTimePeriod& timePeriod)
{
    qint64 endPoint1 = startTimeMs + durationMs;
    qint64 endPoint2 = timePeriod.startTimeMs + timePeriod.durationMs;
    startTimeMs = qMin(startTimeMs, timePeriod.startTimeMs);
    durationMs = qMax(endPoint1, endPoint2) - startTimeMs;
}

QnTimePeriodList QnTimePeriod::agregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs)
{
    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;
    result << periods[0];

    for (int i = 1; i < periods.size(); ++i)
    {
        QnTimePeriod& last = result.last();
        if (periods[i].durationMs != -1 && last.startTimeMs + last.durationMs + detailLevelMs > periods[i].startTimeMs)
            last.durationMs = qMax(last.durationMs, periods[i].startTimeMs + periods[i].durationMs - last.startTimeMs);
        else
            result << periods[i];
    }

    return result;
}


QnTimePeriodList QnTimePeriod::mergeTimePeriods(QVector<QnTimePeriodList> periods)
{
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        for (int i = 0; i < periods.size(); ++i) {
            if (!periods[i].isEmpty() && periods[i][0].startTimeMs < minStartTime) {
                minIndex = i;
                minStartTime = periods[i][0].startTimeMs;
            }
        }

        if (minIndex >= 0)
        {
            // add chunk to merged data
            if (result.isEmpty()) {
                result << periods[minIndex][0];
            }
            else {
                QnTimePeriod& last = result.last();
                if (last.startTimeMs <= minStartTime && last.startTimeMs+last.durationMs >= minStartTime && periods[minIndex][0].durationMs != -1)
                    last.durationMs = qMax(last.durationMs, minStartTime + periods[minIndex][0].durationMs - last.startTimeMs);
                else {
                    result << periods[minIndex][0];
                    if (periods[minIndex][0].durationMs == -1)
                        break;
                }
            } 
            periods[minIndex].pop_front();
        }
    }
    return result;
}

void saveField(QByteArray& stream, qint64 field, quint8 header, int dataLen)
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
void serializeField(QByteArray& stream, qint64 field)
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

bool QnTimePeriod::encode(QByteArray& stream, const QnTimePeriodList& periods)
{
    const quint8* curPtr = (const quint8*) stream.data();

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

qint64 decodeValue(const quint8* &curPtr, const quint8* end)
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

bool QnTimePeriod::decode(const quint8* data, int dataSize, QnTimePeriodList& periods)
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

bool QnTimePeriod::decode(QByteArray& stream, QnTimePeriodList& periods)
{
    return decode((const quint8*) stream.constData(), stream.size(), periods);
}

bool QnTimePeriod::operator==(const QnTimePeriod& other) const
{
    return startTimeMs == other.startTimeMs && durationMs == other.durationMs;
}
