#pragma once

#include "../bookmark_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {

class BookmarkSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(BookmarkSearchListModel* q);
    virtual ~Private() override;

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    virtual void clear() override;

protected:
    virtual rest::Handle requestPrefetch(qint64 latestTimeMs) override;
    virtual bool commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll) override;
    virtual bool hasAccessRights() const override;

private:
    void watchBookmarkChanges();

    void addBookmark(const QnCameraBookmark& bookmark);
    void updateBookmark(const QnCameraBookmark& bookmark);
    void removeBookmark(const QnUuid& guid);

    int indexOf(const QnUuid& guid) const; //< Logarithmic complexity.

    static QPixmap pixmap();
    static QColor color();

private:
    BookmarkSearchListModel* const q = nullptr;
    QnCameraBookmarkList m_prefetch;
    std::deque<QnCameraBookmark> m_data;
    QHash<QnUuid, qint64> m_guidToTimestampMs;
    bool m_success = true;
};

} // namespace
} // namespace client
} // namespace nx
