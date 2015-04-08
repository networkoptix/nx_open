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
    return m_data.empty();
}

void QnTimePeriodCameraData::append(const QnAbstractCameraDataPtr &other) {
    if (!other)
        return;
    append(other->dataSource());
}

void QnTimePeriodCameraData::append(const QList<QnAbstractCameraDataPtr> &other) {
    if (other.isEmpty())
        return;

    std::vector<QnTimePeriodList> allPeriods;
    allPeriods.push_back(m_data);

    for (const QnAbstractCameraDataPtr &other_data: other) {
        if (other_data)
            allPeriods.push_back(other_data->dataSource());
    }
    m_data = QnTimePeriodList::mergeTimePeriods(allPeriods);
}

void QnTimePeriodCameraData::append(const QnTimePeriodList &other) {
    if (other.empty())
        return;

    /* Check if the current last piece marked as Live. */ 
    if (!m_data.empty() && m_data.rbegin()->durationMs == -1) {
        if (other.rbegin()->startTimeMs >= m_data.rbegin()->startTimeMs)
            m_data.pop_back();    //cut "recording" piece
    }
    std::vector<QnTimePeriodList> allPeriods;
    for(const auto& p: m_data)
        allPeriods.push_back(p);
    allPeriods.push_back(other);
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
    if(m_data.empty())
        return false;

    QnTimePeriod& period = *m_data.rbegin();
    if(period.durationMs != -1)
        return false;

    qint64 trimmedDurationMs = qMax(0ll, trimTime - period.startTimeMs);
    if(trimmedDurationMs == 0) {
        m_data.pop_back();
    } else {
        period.durationMs = trimmedDurationMs;
    }

    return true;
}
