// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_groups_widget.h"

namespace nx::vms::client::desktop {

struct UserGroupsWidget::Private
{
    UserGroupsWidget* const q;
};

UserGroupsWidget::UserGroupsWidget(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

UserGroupsWidget::~UserGroupsWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void UserGroupsWidget::loadDataToUi()
{
    // TODO: Implement me!
}

void UserGroupsWidget::applyChanges()
{
    // TODO: Implement me!
}

bool UserGroupsWidget::hasChanges() const
{
    // TODO: Implement me!
    return false;
}

} // namespace nx::vms::client::desktop
