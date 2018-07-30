#pragma once

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

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
