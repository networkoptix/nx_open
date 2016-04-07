#include "user_permissions.h"

#include <utils/common/model_functions.h>

Qn::GlobalPermissions Qn::undeprecate(Qn::GlobalPermissions permissions)
{
    Qn::GlobalPermissions result = permissions;

    if (result.testFlag(Qn::DeprecatedEditCamerasPermission))
    {
        result &= ~Qn::DeprecatedEditCamerasPermission;
        result |= Qn::GlobalEditCamerasPermission | Qn::GlobalPtzControlPermission;
    }

    if (result.testFlag(Qn::DeprecatedViewExportArchivePermission))
    {
        result &= ~Qn::DeprecatedViewExportArchivePermission;
        result |= Qn::GlobalViewArchivePermission | Qn::GlobalExportPermission;
    }

    return result;
}

Qn::Permissions Qn::operator-(Permissions minuend, Permissions subrahend)
{
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}

Qn::Permissions Qn::operator-(Permissions minuend, Permission subrahend)
{
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}

Qn::Permissions Qn::operator-(Permission minuend, Permission subrahend)
{
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}
