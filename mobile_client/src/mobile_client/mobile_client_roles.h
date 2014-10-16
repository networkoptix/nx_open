#ifndef MOBILE_CLIENT_ROLES_H
#define MOBILE_CLIENT_ROLES_H

#include <common/common_globals.h>

namespace Qn {
    enum QnMobileClientRoles {
        ThumbnailRole = RoleCount + 1,
        ServerResourceRole,
        ServerNameRole
    };

    QByteArray roleName(int role);
}

#endif // MOBILE_CLIENT_ROLES_H
