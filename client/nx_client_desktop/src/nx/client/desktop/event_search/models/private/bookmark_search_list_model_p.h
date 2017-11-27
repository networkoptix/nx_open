#pragma once

#include "../bookmark_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {

class BookmarkSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(BookmarkSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    int count() const;
    const QnCameraBookmark& bookmark(int index) const;

    void clear();

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commitPrefetch(qint64 latestStartTimeMs);

    static QPixmap pixmap();
    static QColor color();

private:
    void watchBookmarkChanges();

    void addBookmark(const QnCameraBookmark& bookmark);
    void updateBookmark(const QnCameraBookmark& bookmark);
    void removeBookmark(const QnUuid& guid);

    int indexOf(const QnUuid& guid) const; //< Logarithmic complexity.

private:
    BookmarkSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    bool m_fetchedAll = false;
    bool m_success = true;

    QnCameraBookmarkList m_prefetch;
    std::deque<QnCameraBookmark> m_data;
    QHash<QnUuid, qint64> m_guidToTimestampMs;
};

} // namespace
} // namespace client
} // namespace nx
