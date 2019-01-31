#pragma once

#include "../bookmark_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>

class QnUuid;

namespace nx::vms::client::desktop {

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

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

    void dynamicUpdate(const QnTimePeriod& period);

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

    using GetCallback = std::function<void(bool, const QnCameraBookmarkList&, rest::Handle)>;
    rest::Handle getBookmarks(
        const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit) const;

private:
    int indexOf(const QnUuid& guid) const; //< Logarithmic complexity.

    QnVirtualCameraResourcePtr camera(const QnCameraBookmark& bookmark) const;

    void watchBookmarkChanges();
    void addBookmark(const QnCameraBookmark& bookmark);
    void updateBookmark(const QnCameraBookmark& bookmark);
    void removeBookmark(const QnUuid& id);
    void updatePeriod(const QnTimePeriod& period, const QnCameraBookmarkList& bookmarks);

    template<typename Iter>
    bool commitPrefetch(const QnTimePeriod& periodToCommit,
        Iter prefetchBegin, Iter prefetchEnd, int position);

    static QPixmap pixmap();
    static QColor color();

private:
    BookmarkSearchListModel* const q;
    QString m_filterText;
    QnCameraBookmarkList m_prefetch;
    std::deque<QnCameraBookmark> m_data;
    QHash<QnUuid, std::chrono::milliseconds> m_guidToTimestamp;
    QHash<rest::Handle, QnTimePeriod> m_updateRequests;
};

} // namespace nx::vms::client::desktop
