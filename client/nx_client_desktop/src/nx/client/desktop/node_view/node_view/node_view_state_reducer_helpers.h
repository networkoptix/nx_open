#pragma once

#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;
class NodeViewStatePatch;

namespace details {

void addCheckStateChangeToPatch(
    NodeViewStatePatch& patch,
    const ViewNodePath& path,
    const ColumnsSet& columns,
    Qt::CheckState state);

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
