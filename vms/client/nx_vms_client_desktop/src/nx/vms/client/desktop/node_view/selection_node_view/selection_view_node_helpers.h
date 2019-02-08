#pragma once

#include "../details/node/view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {

bool checkAllNode(const details::NodePtr& node);

details::NodePtr createCheckAllNode(
    const details::ColumnSet& selectionColumns,
    int mainColumn,
    const QString& text,
    const QIcon& icon = QIcon(),
    int groupSortOrder = 0);

int selectedChildrenCount(const details::NodePtr& node);

void setSelectedChildrenCount(details::ViewNodeData& data, int count);

} // namespace node_view
} // namespace nx::vms::client::desktop
