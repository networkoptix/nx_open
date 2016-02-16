#include "user_permissions.h"

#include <utils/common/model_functions.h>

Qn::Permissions Qn::undeprecate(Permissions permissions)
{
    Qn::Permissions result = permissions;

    if(result & Qn::DeprecatedEditCamerasPermission) {
        result &= ~Qn::DeprecatedEditCamerasPermission;
        result |= Qn::GlobalEditCamerasPermission | Qn::GlobalPtzControlPermission;
    }

    if(result & Qn::DeprecatedViewExportArchivePermission) {
        result &= ~Qn::DeprecatedViewExportArchivePermission;
        result |= Qn::GlobalViewArchivePermission | Qn::GlobalExportPermission;
    }

    if(result & Qn::GlobalProtectedPermission)
        result |= Qn::GlobalPanicPermission;

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
