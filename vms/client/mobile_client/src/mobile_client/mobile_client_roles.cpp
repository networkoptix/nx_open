#include "mobile_client_roles.h"

#include <nx/utils/log/assert.h>

QByteArray Qn::roleName(int role)
{
    switch (role)
    {
        case ResourceNameRole:
            return "resourceName";
        case ResourceStatusRole:
            return "resourceStatus";
        case ThumbnailRole:
            return "thumbnail";
        case UuidRole:
            return "uuid";
        case IpAddressRole:
            return "ipAddress";
        default:
            NX_ASSERT(0, "Unsupported role", Q_FUNC_INFO);
    }
    return QByteArray();
}
