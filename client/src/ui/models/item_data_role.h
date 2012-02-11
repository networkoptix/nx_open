#ifndef QN_RESOURCE_ROLE_H
#define QN_RESOURCE_ROLE_H

#include <Qt>

namespace Qn {

    enum ItemDataRole {
        ResourceRole        = Qt::UserRole + 1, /**< Role for QnResourcePtr. */
        ResourceFlagsRole   = Qt::UserRole + 2, /**< Role for resource flags. */
        IdRole              = Qt::UserRole + 3, /**< Role for resource's QnId. */
        UuidRole            = Qt::UserRole + 4, /**< Role for layout item's UUID. */
        SearchStringRole    = Qt::UserRole + 5, /**< Role for search string. */
        StatusRole          = Qt::UserRole + 6, /**< Role for resource's status. */
    };



} // namespace Qn

#endif // QN_RESOURCE_ROLE_H
