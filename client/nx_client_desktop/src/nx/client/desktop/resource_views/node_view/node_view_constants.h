#pragma once

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

enum Column
{
    nameColumn,
    checkMarkColumn,

    columnCount
};

enum Role
{
    resourceRole = Qt::UserRole,
    separatorRole,
    siblingGroupRole
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
