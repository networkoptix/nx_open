#include "time_period_list.h"

#include <utils/common/util.h>
#include <utils/math/math.h>

QnTimePeriodList::const_iterator QnTimePeriodList::findNearestPeriod(qint64 timeMs, bool searchForward) const {
    if (isEmpty())
        return end();

    const_iterator itr = qUpperBound(constBegin(), constEnd(), timeMs);
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
    if (period != constEnd()) {
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

    if(back().durationMs == -1)
        return -1;

    qint64 result = 0;
    foreach(const QnTimePeriod &period, *this)
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

bool QnTimePeriodList::encode(QByteArray &stream)
{
    if (isEmpty())
        return true;
    qint64 timePos = first().startTimeMs;

    qint64 val = htonll(timePos << 16);
    stream.append(((const char*) &val), 6);

    for (int i = 0; i < size(); ++i)
    {
        qint64 timeDelta = at(i).startTimeMs - timePos;
        if (timeDelta < 0)
            return false;
        if (i > 0)
            serializeField(stream, timeDelta);
        serializeField(stream, at(i).durationMs+1);
        timePos += timeDelta + at(i).durationMs;
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
    while (relStartTime != -1)
    {
        qint64 duration = decodeValue(curPtr, end);
        if (duration == -1) 
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
    return decode((const quint8 *) stream.constData(), stream.size());
}


QnGenericTimePeriodList::QnGenericTimePeriodList() {

}

QnGenericTimePeriodList::QnGenericTimePeriodList(const QnTimePeriodList &data):
    m_data(data)
{
}

bool QnGenericTimePeriodList::isEmpty() const {
    return m_data.isEmpty();
}

void QnGenericTimePeriodList::append(const QnAbstractTimePeriodListPtr &other) {
    if (!other)
        return;
    QnTimePeriodList otherList;
    if (QnGenericTimePeriodList* other_casted = dynamic_cast<QnGenericTimePeriodList*>(other.data()))
        otherList = other_casted->m_data;
    if (m_data.isEmpty())
        return;

    QVector<QnTimePeriodList> allPeriods;
    if (!otherList.isEmpty() && !m_data.isEmpty() && m_data.last().durationMs == -1) 
        if (otherList.last().startTimeMs >= m_data.last().startTimeMs) 
            m_data.last().durationMs = 0;
    allPeriods << m_data << otherList;
    m_data = QnTimePeriod::mergeTimePeriods(allPeriods); // union data
}

QnTimePeriod QnGenericTimePeriodList::last() const {
    if (m_data.isEmpty())
        return QnTimePeriod();
    return m_data.last();
}

QnAbstractTimePeriodListPtr QnGenericTimePeriodList::merged(const QVector<QnAbstractTimePeriodListPtr> &source) {
    QVector<QnTimePeriodList> allPeriods;

    foreach (const QnAbstractTimePeriodListPtr &other, source) {
        if (!other)
            continue;
        if (QnGenericTimePeriodList* other_casted = dynamic_cast<QnGenericTimePeriodList*>(other.data()))
            allPeriods << other_casted->m_data;
    }
    return QnGenericTimePeriodListPtr(new QnGenericTimePeriodList(QnTimePeriod::mergeTimePeriods(allPeriods)));
}
