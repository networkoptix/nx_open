#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

#include <ui/workbench/workbench_navigator.h>

namespace nx::client::desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        d, &Private::updateBookmarksWatcher);
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

} // namespace nx::client::desktop
