#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
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

bool BookmarkSearchListModel::hasAccessRights() const
{
    return accessController()->hasGlobalPermission(GlobalPermission::viewBookmarks);
}

void BookmarkSearchListModel::dynamicUpdate(const QnTimePeriod& period)
{
    d->dynamicUpdate(period);
}

} // namespace nx::vms::client::desktop
