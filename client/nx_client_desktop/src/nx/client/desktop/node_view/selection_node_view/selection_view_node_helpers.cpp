#include "selection_view_node_helpers.h"

#include "../details/node/view_node.h"

namespace {

static constexpr int kCheckAllNodeRoleId = Qt::UserRole + 1000;

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

bool isCheckAllNode(const details::NodePtr& node)
{
     return node && node->commonNodeData(kCheckAllNodeRoleId).toBool();
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

