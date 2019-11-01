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
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
