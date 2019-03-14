#pragma once

#include "../details/node/view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {

int selectedChildrenCount(const details::NodePtr& node);

void setSelectedChildrenCount(details::ViewNodeData& data, int count);

} // namespace node_view
} // namespace nx::vms::client::desktop
