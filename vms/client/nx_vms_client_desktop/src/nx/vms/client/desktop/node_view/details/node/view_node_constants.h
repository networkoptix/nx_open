#pragma once

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

enum ViewDataProperty
{
    isSeparatorProperty,
    isExpandedProperty,
    groupSortOrderProperty,

    lastNodeViewProperty = 128 //< All other properties should start at least from this value.
};

enum CustomViewNodeRole
{
    firstCustomViewNodeRole = Qt::UserRole,

    progressRole = firstCustomViewNodeRole,
    useSwitchStyleForCheckboxRole,

    lastRole,
    lastViewNodeRole = lastRole + 1024,

    userChangesRoleOffset = Qt::UserRole + 10000
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
