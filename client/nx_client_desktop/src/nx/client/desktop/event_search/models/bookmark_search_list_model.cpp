#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(d_func()))
{
}

QString BookmarkSearchListModel::filterText() const
{
    return d->filterText();
}
void BookmarkSearchListModel::setFilterText(const QString& value)
{
    d->setFilterText(value);
}

bool BookmarkSearchListModel::isConstrained() const
{
    return !filterText().isEmpty() || base_type::isConstrained();
}

} // namespace desktop
} // namespace client
} // namespace nx
