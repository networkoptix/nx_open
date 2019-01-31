#pragma once

#include "../details/node/view_node_constants.h"

namespace nx::vms::client::desktop {
namespace node_view {

enum SelectionNodeViewProperty
{
    checkAllNodeProperty = details::lastNodeViewProperty,
    selectedChildrenCountProperty,

    lastSelectionNodeViewProperty = details::lastNodeViewProperty + 128
};

} // namespace node_view
} // namespace nx::vms::client::desktop
