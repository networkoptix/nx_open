#pragma once

#include "../selection_node_view/selection_node_view_constants.h"

namespace nx::vms::client::desktop {
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
    resourceExtraTextRole,
    cameraExtraStatusRole
};

enum ResourceNodeViewProperty
{
    validResourceProperty = lastSelectionNodeViewProperty,

    lastResourceNodeViewProperty = lastSelectionNodeViewProperty + 128
};

} // namespace node_view
} // namespace nx::vms::client::desktop
