#include "bookmark_camera_data.h"

#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

namespace {

    QnCameraBookmarkList mergeBookmarks(const QVector<QnCameraBookmarkList> lists) {
        if(lists.size() == 1)
            return lists[0];

        qDebug() << "merging bookmark lists" << lists;

        QVector<int> minIndexes;
        minIndexes.resize(lists.size());
        QnCameraBookmarkList result;
        int minIndex = 0;
        while (minIndex != -1)
        {
            QnCameraBookmark min; min.startTimeMs = 0x7fffffffffffffffll;
            minIndex = -1;
            for (int i = 0; i < lists.size(); ++i) 
            {
                int startIdx = minIndexes[i];
                if (startIdx < lists[i].size() && lists[i][startIdx] < min) {
                    minIndex = i;
                    min = lists[i][startIdx];
                }
            }

            if (minIndex >= 0)
            {
                int& startIdx = minIndexes[minIndex];
                // add item to merged data

                if (result.isEmpty()) {
                    result << lists[minIndex][startIdx];
                } else {
                    QnCameraBookmark &last = result.last();
                    if (last.guid != lists[minIndex][startIdx].guid)
                        result << lists[minIndex][startIdx];
                }
                startIdx++;
            }
        }

        qDebug() << "merge result" << result;
        return result;
    }

}

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
    m_dataSource = QnTimePeriodList::mergeTimePeriods(periods); //TODO: #GDM #Bookmarks need an analogue for the single periods
}

void QnBookmarkCameraData::append(const QnAbstractCameraDataPtr &other) {
    if (!other)
        return;

    QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data());
    if (!other_casted)
        return;

    QVector<QnCameraBookmarkList> lists;
    lists.append(m_data);
    lists.append(other_casted->m_data);
    m_data = mergeBookmarks(lists);

    QVector<QnTimePeriodList> periods;
    periods.append(m_dataSource);
    periods.append(other_casted->m_dataSource);
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
    QVector<QnCameraBookmarkList> lists;
    lists.append(m_data);
    foreach (const QnAbstractCameraDataPtr &other, source) {
        if (QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data()))
            lists.append(other_casted->m_data);
    }
    return QnAbstractCameraDataPtr(new QnBookmarkCameraData(mergeBookmarks(lists)));
}

bool QnBookmarkCameraData::contains(const QnAbstractCameraDataPtr &other) const {
    QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data());
    if (!other_casted)
        return false;
    foreach (const QnCameraBookmark &bookmark, other_casted->m_data)
        if (!m_data.contains(bookmark))
            return false;
    return true;
}

bool QnBookmarkCameraData::isEmpty() const {
    return m_data.isEmpty();
}

