#include "selection_view_node_helpers.h"

#include "selection_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

bool checkAllNode(const NodePtr& node)
{
     return node && node->property(checkAllNodeProperty).toBool();
}

NodePtr createCheckAllNode(
    const details::ColumnSet& selectionColumns,
    int mainColumn,
    const QString& text,
    const QIcon& icon,
    int groupSortOrder)
{
    auto data = ViewNodeDataBuilder()
        .withText(mainColumn, text)
        .withIcon(mainColumn, icon)
        .withCheckedState(selectionColumns, Qt::Unchecked)
        .withGroupSortOrder(groupSortOrder)
        .data();
    data.setProperty(checkAllNodeProperty, true);
    return ViewNode::create(data);
}

int selectedChildrenCount(const NodePtr& node)
{
    return node ? node->property(selectedChildrenCountProperty).toInt() : 0;
}

void setSelectedChildrenCount(ViewNodeData& data, int count)
{
    data.setProperty(selectedChildrenCountProperty, count);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
