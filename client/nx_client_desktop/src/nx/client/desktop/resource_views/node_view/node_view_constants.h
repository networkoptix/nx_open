#pragma once

#include <QtCore/QFlags>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

enum NodeDataRole
{
    resourceRole = Qt::UserRole, //< TODO: move resourceRole to the disctinct file
    extraTextRole
};

enum CommonNodeRole
{
    allSiblingsCheckModeCommonRole= Qt::UserRole,
    separatorCommonRole,
    expandedCommonRole,
    siblingGroupCommonRole
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
