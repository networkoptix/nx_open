#ifndef QN_RESOURCE_ROLE_H
#define QN_RESOURCE_ROLE_H

#include <Qt>

namespace Qn {

    enum NodeType {
        RootNode,
        LocalNode,
        ServersNode,
        UsersNode,
        ResourceNode,   /**< Node that represents a resource. */
        ItemNode,       /**< Node that represents a layout item. */
    };

    enum ItemDataRole {
        ResourceRole        = Qt::UserRole + 1, /**< Role for QnResourcePtr. */
        ResourceFlagsRole   = Qt::UserRole + 2, /**< Role for resource flags. */
        UuidRole            = Qt::UserRole + 3, /**< Role for layout item's UUID. */
        SearchStringRole    = Qt::UserRole + 4, /**< Role for search string. */
        StatusRole          = Qt::UserRole + 5, /**< Role for resource's status. */
        NodeTypeRole        = Qt::UserRole + 6, /**< Role for node type, see <tt>Qn::NodeType</tt>. */
    };


} // namespace Qn

#endif // QN_RESOURCE_ROLE_H
