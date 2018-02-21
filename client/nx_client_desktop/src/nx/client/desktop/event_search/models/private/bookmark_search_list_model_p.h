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

namespace utils { class PendingOperation; }

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

    QString filterText() const;
    void setFilterText(const QString& value);

    virtual void clear() override;

protected:
    virtual rest::Handle requestPrefetch(qint64 fromMs, qint64 toMs) override;
    virtual bool commitPrefetch(qint64 syncTimeToCommitMs, bool& fetchedAll) override;
    virtual void clipToSelectedTimePeriod() override;
    virtual bool hasAccessRights() const override;

private:
    void watchBookmarkChanges();

    void addBookmark(const QnCameraBookmark& bookmark);
    void updateBookmark(const QnCameraBookmark& bookmark);
    void removeBookmark(const QnUuid& guid);

    int indexOf(const QnUuid& guid) const; //< Logarithmic complexity.

    utils::PendingOperation* createUpdateBookmarksWatcherOperation();

    static QPixmap pixmap();
    static QColor color();

private:
    BookmarkSearchListModel* const q = nullptr;
    QString m_filterText;
    QnCameraBookmarkList m_prefetch;
    QScopedPointer<utils::PendingOperation> m_updateBookmarksWatcher;
    std::deque<QnCameraBookmark> m_data;
    QHash<QnUuid, qint64> m_guidToTimestampMs;
    bool m_success = true;
};

} // namespace desktop
} // namespace client
} // namespace nx
