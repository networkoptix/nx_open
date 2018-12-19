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

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
