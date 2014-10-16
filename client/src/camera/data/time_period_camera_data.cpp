#include "time_period_camera_data.h"

#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

QnTimePeriodCameraData::QnTimePeriodCameraData():
    QnAbstractCameraData()
{
}

QnTimePeriodCameraData::QnTimePeriodCameraData(const QnTimePeriodList &data):
    QnAbstractCameraData(),
    m_data(data)
{
}


QnAbstractCameraDataPtr QnTimePeriodCameraData::clone() const {
    QnAbstractCameraDataPtr result(new QnTimePeriodCameraData(m_data));
    return result;
}

bool QnTimePeriodCameraData::isEmpty() const {
    return m_data.isEmpty();
}

void QnTimePeriodCameraData::append(const QnAbstractCameraDataPtr &other) {
    if (!other)
        return;
    append(other->dataSource());
}

void QnTimePeriodCameraData::append(const QList<QnAbstractCameraDataPtr> &other) {
    if (other.isEmpty())
        return;

    QVector<QnTimePeriodList> allPeriods;
    allPeriods << m_data;

    foreach (const QnAbstractCameraDataPtr &other_data, other)
        if (other_data)
            allPeriods << other_data->dataSource();
    m_data = QnTimePeriodList::mergeTimePeriods(allPeriods);
}

void QnTimePeriodCameraData::append(const QnTimePeriodList &other) {
    if (other.isEmpty())
        return;

    /* Check if the current last piece marked as Live. */ 
    QVector<QnTimePeriodList> allPeriods;
    if (!m_data.isEmpty() && m_data.last().durationMs == -1) 
                if (other.last().startTimeMs >= m_data.last().startTimeMs)
        m_data.removeLast();    //cut "recording" piece
    allPeriods << m_data << other;
    m_data = QnTimePeriodList::mergeTimePeriods(allPeriods); // union data
}

void QnTimePeriodCameraData::clear() {
    m_data.clear();
}

bool QnTimePeriodCameraData::contains(const QnAbstractCameraDataPtr &other) const {
    if (!other)
        return true;
    foreach (const QnTimePeriod &period, other->dataSource())
        if (!m_data.containPeriod(period))
            return false;
    return true;
}

QnTimePeriodList QnTimePeriodCameraData::dataSource() const  {
    return m_data;
}

bool QnTimePeriodCameraData::trim(qint64 trimTime) {
    if(m_data.isEmpty())
        return false;

    QnTimePeriod period = m_data.last();
    if(period.durationMs != -1)
        return false;

    qint64 trimmedDurationMs = qMax(0ll, trimTime - period.startTimeMs);
    period.durationMs = trimmedDurationMs;
    if(period.durationMs == 0) {
        m_data.pop_back();
    } else {
        m_data.back() = period;
    }

    return true;
}
