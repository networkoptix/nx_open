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
    nodeFlagsRole,
    separatorRole, //TODO: move to flags
    siblingGroupRole //TODO: move to flags
};

enum NodeFlag
{
    AllSiblingsCheckFlag
};

Q_DECLARE_FLAGS(NodeFlags, NodeFlag);

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::node_view::NodeFlags);
