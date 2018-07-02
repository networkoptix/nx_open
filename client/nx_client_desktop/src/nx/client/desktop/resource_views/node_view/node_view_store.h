#pragma once

#include <nx/client/desktop/utils/base_store.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStore: public BaseStore<NodeViewState>
{
public:
    void setNodeChecked(const ViewNode::Path& path, Qt::CheckState checkedState);
};

} // namespace desktop
} // namespace client
} // namespace nx
