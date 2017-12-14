#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

#include <core/resource/camera_resource.h>
#include <ui/help/help_topics.h>

namespace nx {
namespace client {
namespace desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

BookmarkSearchListModel::~BookmarkSearchListModel()
{
}

QnVirtualCameraResourcePtr BookmarkSearchListModel::camera() const
{
    return d->camera();
}

void BookmarkSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

void BookmarkSearchListModel::clear()
{
    d->clear();
}

int BookmarkSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant BookmarkSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    const auto& bookmark = d->bookmark(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
            return bookmark.name;

        case Qt::DecorationRole:
            return QVariant::fromValue(d->pixmap());

        case Qt::ForegroundRole:
            return QVariant::fromValue(d->color());

        case Qn::DescriptionTextRole:
            return bookmark.description;

        case Qn::TimestampRole:
        case Qn::PreviewTimeRole:
            return QVariant::fromValue(bookmark.startTimeMs);

        case Qn::UuidRole:
            return QVariant::fromValue(bookmark.guid);

        case Qn::HelpTopicIdRole:
            return Qn::Bookmarks_Usage_Help;

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->camera());

        default:
            return base_type::data(index, role);
    }
}

bool BookmarkSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

bool BookmarkSearchListModel::prefetchAsync(PrefetchCompletionHandler completionHandler)
{
    return d->prefetch(completionHandler);
}

void BookmarkSearchListModel::commitPrefetch(qint64 keyLimitFromSync)
{
    d->commitPrefetch(keyLimitFromSync);
}

bool BookmarkSearchListModel::fetchInProgress() const
{
    return d->fetchInProgress();
}

} // namespace
} // namespace client
} // namespace nx
