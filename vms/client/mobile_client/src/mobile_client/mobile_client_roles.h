#pragma once

#include <QtCore/QByteArray>

namespace Qn {

enum QnMobileClientRoles
{
    ThumbnailRole = Qt::UserRole + 1,
    ServerResourceRole,
    ServerNameRole,
    IpAddressRole,
    ResourceNameRole,
    ResourceStatusRole,
    UuidRole,
    ResourceRole,
    LayoutResourceRole,
    MediaServerResourceRole,

    RoleCount
};

QByteArray roleName(int role);

} // namespace Qn
