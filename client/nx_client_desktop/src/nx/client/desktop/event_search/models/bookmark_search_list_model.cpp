#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(d_func()))
{
    // TODO: FIXME!!! REMOVE
    setFetchBatchSize(10);
    setMaximumCount(10);
}

QString BookmarkSearchListModel::filterText() const
{
    return d->filterText();
}
void BookmarkSearchListModel::setFilterText(const QString& value)
{
    d->setFilterText(value);
}

void BookmarkSearchListModel::truncateToMaximumCount()
{
    // TODO: #vkutin Fixme!!
}

} // namespace desktop
} // namespace client
} // namespace nx
