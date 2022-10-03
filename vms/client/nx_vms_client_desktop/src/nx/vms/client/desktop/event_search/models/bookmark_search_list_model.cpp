// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
}

TextFilterSetup* BookmarkSearchListModel::textFilter() const
{
    return d->textFilter.get();
}

bool BookmarkSearchListModel::isConstrained() const
{
    return !d->textFilter->text().isEmpty() || base_type::isConstrained();
}

bool BookmarkSearchListModel::hasAccessRights() const
{
    return accessController()->anyResourceHasPermissions(Qn::Permission::ViewBookmarksPermission);
}

void BookmarkSearchListModel::dynamicUpdate(const QnTimePeriod& period)
{
    d->dynamicUpdate(period);
}

} // namespace nx::vms::client::desktop
