#pragma once

#include "../details/node/view_node_constants.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

enum SelectionNodeViewProperty
{
    checkAllNodeProperty = details::lastNodeViewProperty,
    selectedChildrenCountProperty,

    lastSelectionNodeViewProperty = details::lastNodeViewProperty + 128
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
