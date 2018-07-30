#pragma once

#include "../details/node/view_node_fwd.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

namespace details {

class ViewNodePath;
class NodeViewStatePatch;

} // namespace details

void addCheckStateChangeToPatch(
    details::NodeViewStatePatch& patch,
    const details::ViewNodePath& path,
    const details::ColumnsSet& columns,
    Qt::CheckState state);

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
