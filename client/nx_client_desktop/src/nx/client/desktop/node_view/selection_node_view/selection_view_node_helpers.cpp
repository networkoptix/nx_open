#include "selection_view_node_helpers.h"

#include "../details/node/view_node.h"
#include "../details/node/view_node_data_builder.h"

namespace {

static constexpr int kCheckAllNodeRoleId = Qt::UserRole + 1000;

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

bool isCheckAllNode(const NodePtr& node)
{
     return node && node->property(kCheckAllNodeRoleId).toBool();
}

NodePtr createCheckAllNode(
    const details::ColumnsSet& selectionColumns,
    int mainColumn,
    const QString& text,
    const QIcon& icon,
    int siblingGroup)
{
    auto data = ViewNodeDataBuilder()
        .withText(mainColumn, text)
        .withIcon(mainColumn, icon)
        .withCheckedState(selectionColumns, Qt::Unchecked)
        .withSiblingGroup(siblingGroup)
        .data();
    data.setProperty(kCheckAllNodeRoleId, true);
    return ViewNode::create(data);
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

