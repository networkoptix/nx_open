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

void QnTimePeriodCameraData::append(const QnAbstractCameraDataPtr &other) {
    if (!other)
        return;
    if (QnTimePeriodCameraData* other_casted = dynamic_cast<QnTimePeriodCameraData*>(other.data()))
        append(other_casted->m_data);
}

void QnTimePeriodCameraData::append(const QnTimePeriodList &other) {
    if (other.isEmpty())
        return;

    QVector<QnTimePeriodList> allPeriods;
    if (!m_data.isEmpty() && m_data.last().durationMs == -1) 
        if (other.last().startTimeMs >= m_data.last().startTimeMs) // TODO: #Elric should this be otherList.last().startTimeMs?
            m_data.last().durationMs = 0;
    allPeriods << m_data << other;
    m_data = QnTimePeriodList::mergeTimePeriods(allPeriods); // union data
}


QnAbstractCameraDataPtr QnTimePeriodCameraData::merge(const QVector<QnAbstractCameraDataPtr> &source) {
    QVector<QnTimePeriodList> allPeriods;
    allPeriods << m_data;

    foreach (const QnAbstractCameraDataPtr &other, source) {
        if (!other)
            continue;
        if (QnTimePeriodCameraData* other_casted = dynamic_cast<QnTimePeriodCameraData*>(other.data()))
            allPeriods << other_casted->m_data;
    }
    return QnAbstractCameraDataPtr(new QnTimePeriodCameraData(QnTimePeriodList::mergeTimePeriods(allPeriods)));
}

bool QnTimePeriodCameraData::contains(const QnAbstractCameraDataPtr &other) const {
    foreach (const QnTimePeriod &period, other->dataSource())
        if (!m_data.containPeriod(period))
            return false;
    return true;
}

QnTimePeriodList QnTimePeriodCameraData::dataSource() const  {
    return m_data;
}

void QnTimePeriodCameraData::clear() {
    m_data.clear();
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
