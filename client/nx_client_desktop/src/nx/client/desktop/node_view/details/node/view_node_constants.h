#pragma once

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

enum ViewDataProperty
{
    isSeparatorProperty,
    isExpandedProperty,
    siblingGroupProperty,

    lastNodeViewProperty = 128 //< All other properties should start at least from this value.
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
