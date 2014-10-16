#include "mobile_client_roles.h"


QByteArray Qn::roleName(int role) {
    switch (role) {
    case NodeTypeRole:
        return "nodeType";
    case ThumbnailRole:
        return "thumbnail";
    case UuidRole:
        return "uuid";
    default:
        Q_ASSERT_X(0, "Unsupported role", Q_FUNC_INFO);
    }
    return QByteArray();
}
