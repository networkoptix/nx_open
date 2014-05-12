#include "bookmark_camera_data.h"

#include <core/resource/camera_bookmark.h>

namespace {

    QnCameraBookmarkList mergeBookmarks(const QVector<QnCameraBookmarkList> lists) {
        if(lists.size() == 1)
            return lists[0];

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

        return result;
    }

}

QnBookmarkCameraData::QnBookmarkCameraData():
    QnAbstractCameraData()
{
}

QnBookmarkCameraData::QnBookmarkCameraData(const QnCameraBookmarkList &data):
    QnAbstractCameraData(),
    m_data(data)
{
    updateDataSource();
}

bool QnBookmarkCameraData::isEmpty() const {
    return m_data.isEmpty();
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

void QnBookmarkCameraData::append(const QList<QnAbstractCameraDataPtr> &other) {
    if (other.isEmpty())
        return;

    QVector<QnCameraBookmarkList> lists;
    lists.append(m_data);
    foreach (const QnAbstractCameraDataPtr &other_data, other) {
        if (QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other_data.data()))
            lists.append(other_casted->m_data);
    }
    m_data = mergeBookmarks(lists);
    updateDataSource();
}

void QnBookmarkCameraData::clear() {
    m_data.clear();
    m_dataSource.clear();
}

bool QnBookmarkCameraData::contains(const QnAbstractCameraDataPtr &other) const {
    QnBookmarkCameraData* other_casted = dynamic_cast<QnBookmarkCameraData*>(other.data());
    if (!other_casted)
        return false;

    QSet<QUuid> lookedUp;
    foreach (const QnCameraBookmark &bookmark, other_casted->m_data)
        lookedUp.insert(bookmark.guid);

    foreach (const QnCameraBookmark &bookmark, m_data)
        lookedUp.remove(bookmark.guid);

    return lookedUp.isEmpty();
}

QnTimePeriodList QnBookmarkCameraData::dataSource() const {
    return m_dataSource;
}

QnCameraBookmark QnBookmarkCameraData::find(const qint64 position) const {
    QnCameraBookmark result;
    foreach (const QnCameraBookmark &bookmark, m_data) {
        // stop if we overcome the position, bookmarks are sorted by start time
        if (bookmark.startTimeMs > position)
            break;

        if (bookmark.endTimeMs() >= position && bookmark.durationMs > result.durationMs)
            result = bookmark;
    }
    return result;
}

QnCameraBookmarkList QnBookmarkCameraData::data() const {
    return m_data;
}

void QnBookmarkCameraData::updateBookmark(const QnCameraBookmark &bookmark) {
    int idx = -1;
    for (int i = 0; i < m_data.size(); ++i) {
        QnCameraBookmark existing = m_data[i];
        // stop if we overcome the position, bookmarks are sorted by start time
        if (existing.startTimeMs > bookmark.startTimeMs)
            break;

        if (existing.guid != bookmark.guid)
            continue;
        idx = i;
        break;
    }

    if (idx < 0) {
        qWarning() << "updating non-existent bookmark" << bookmark;
        return;
    }

    bool dataSourceChanged = m_data[idx].startTimeMs != bookmark.startTimeMs ||
            m_data[idx].durationMs != bookmark.durationMs;
    m_data[idx] = bookmark;

    if (!dataSourceChanged) 
        return;

    //TODO: #GDM #Bookmarks Implement binary search sometime. Lowest priority because is not used at all for now.
    // try to move bookmark backward to maintain sorting order
    int i = idx;
    while (i > 0 && m_data[i - 1].startTimeMs > bookmark.startTimeMs)
        i--;
    if (i == idx) // try to move bookmark forward to maintain sorting order
        while (i < m_data.size() - 1 && m_data[i + 1].startTimeMs < bookmark.startTimeMs)
            i++;

    if (i != idx)
        m_data.move(idx, i);
    updateDataSource();
}

void QnBookmarkCameraData::removeBookmark(const QnCameraBookmark & bookmark) {
    m_data.erase(std::remove_if(m_data.begin(), m_data.end(), [bookmark](const QnCameraBookmark &value){ return value.guid == bookmark.guid; }), m_data.end());
    updateDataSource();
}

void QnBookmarkCameraData::updateDataSource() {
    QVector<QnTimePeriodList> periods;
    foreach (const QnCameraBookmark &bookmark, m_data)
        periods.append(QnTimePeriodList(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs)));
    m_dataSource = QnTimePeriodList::mergeTimePeriods(periods); //TODO: #GDM #Bookmarks need an analogue for the single periods
}


