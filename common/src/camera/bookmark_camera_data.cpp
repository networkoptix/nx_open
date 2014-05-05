#include "bookmark_camera_data.h"

#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

QnBookmarkCameraData::QnBookmarkCameraData():
    QnAbstractCameraData(Qn::BookmarkData)
{
}

QnBookmarkCameraData::QnBookmarkCameraData(const QnCameraBookmarkList &data):
    QnAbstractCameraData(Qn::BookmarkData),
    m_data(data)
{
    QVector<QnTimePeriodList> periods;
    foreach (const QnCameraBookmark &bookmark, m_data)
        periods.append(QnTimePeriodList(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs)));
    m_dataSource = QnTimePeriodList::mergeTimePeriods(periods); //TODO: #GDM need an analogue for the single periods
}

void QnBookmarkCameraData::append(const QnAbstractCameraDataPtr &other) {
    if (!other)
        return;
    QnCameraBookmarkList otherList;
    if (QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data()))
        otherList = other_casted->m_data;
    m_data.append(otherList);   //TODO: #GDM merge sort

    QVector<QnTimePeriodList> periods;
    periods.append(m_dataSource);
    foreach (const QnCameraBookmark &bookmark, otherList)
        periods.append(QnTimePeriodList(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs)));

    m_dataSource = QnTimePeriodList::mergeTimePeriods(periods);
}

QnTimePeriodList QnBookmarkCameraData::dataSource() const {
    return m_dataSource;
}

void QnBookmarkCameraData::clear() {
    m_data.clear();
    m_dataSource.clear();
}

QnAbstractCameraDataPtr QnBookmarkCameraData::merge(const QVector<QnAbstractCameraDataPtr> &source) {
    QnCameraBookmarkList merged(m_data);
    foreach (const QnAbstractCameraDataPtr &other, source) {
        if (QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data()))
            merged.append(other_casted->m_data);
    }
    return QnAbstractCameraDataPtr(new QnBookmarkCameraData(merged)); //TODO: #GDM implement real merge
}

bool QnBookmarkCameraData::operator==(const QnAbstractCameraDataPtr &other) const {
    throw std::exception();
}

bool QnBookmarkCameraData::isEmpty() const {
    return m_data.isEmpty();
}

