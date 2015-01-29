#include "mobile_client_roles.h"


QByteArray Qn::roleName(int role) {
    switch (role) {
    case Qn::ResourceNameRole:
        return "resourceName";
    case NodeTypeRole:
        return "nodeType";
    case ThumbnailRole:
        return "thumbnail";
    case UuidRole:
        return "uuid";
    case IpAddressRole:
        return "ipAddress";
    default:
        Q_ASSERT_X(0, "Unsupported role", Q_FUNC_INFO);
    }
    return QByteArray();
}
