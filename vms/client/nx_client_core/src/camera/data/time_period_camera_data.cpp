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

void QnTimePeriodCameraData::update(const QnAbstractCameraDataPtr &other, const QnTimePeriod &updatedPeriod) {
    if (!other)
        return;
    update(other->dataSource(), updatedPeriod);
}

void QnTimePeriodCameraData::mergeInto(const QList<QnAbstractCameraDataPtr> &other) {
    if (other.isEmpty())
        return;

    std::vector<QnTimePeriodList> allPeriods;
    allPeriods.push_back(m_data);

    foreach (const QnAbstractCameraDataPtr &other_data, other)
        if (other_data)
            allPeriods.push_back(other_data->dataSource());
    m_data = QnTimePeriodList::mergeTimePeriods(allPeriods);
}

void QnTimePeriodCameraData::update(const QnTimePeriodList &other, const QnTimePeriod &updatedPeriod) {
    /* Update data, received from one server and related to one camera. */
    QnTimePeriodList::overwriteTail(m_data, other, updatedPeriod.startTimeMs);
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

    QnTimePeriod& period = m_data.last();
    if(!period.isInfinite())
        return false;

    qint64 trimmedDurationMs = qMax(0ll, trimTime - period.startTimeMs);
    period.durationMs = trimmedDurationMs;
    if(trimmedDurationMs == 0) {
        m_data.pop_back();
    } else {
        period.durationMs = trimmedDurationMs;
    }

    return true;
}
