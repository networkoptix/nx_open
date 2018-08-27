#pragma once

#include "../selection_node_view/selection_node_view_constants.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

enum ResourceNodeViewColumn
{
    resourceNameColumn,
    resourceCheckColumn,
    resourceColumnsCount
};

enum ResourceNodeDataRole
{
    resourceRole = Qt::UserRole,
    resourceExtraTextRole
};

enum ResourceNodeViewProperty
{
    validResourceProperty = lastSelectionNodeViewProperty,

    lastResourceNodeViewProperty = lastSelectionNodeViewProperty + 128
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
