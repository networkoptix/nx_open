#pragma once

#include "../details/node/view_node_fwd.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

bool isCheckAllNode(const details::NodePtr& node);

details::NodePtr createCheckAllNode(
    const details::ColumnsSet& selectionColumns,
    int mainColumn,
    const QString& text,
    const QIcon& icon = QIcon(),
    int siblingGroup = 0);

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
