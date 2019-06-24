#include "resource_tree_globals.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace ResourceTree {

bool isSeparatorNode(NodeType nodeType)
{
    return nodeType == NodeType::separator
        || nodeType == NodeType::localSeparator;
}

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "ResourceTree", "ResourceTree is a namespace");
}

} // namespace ResourceTree
} // namespace nx::vms::client::desktop
