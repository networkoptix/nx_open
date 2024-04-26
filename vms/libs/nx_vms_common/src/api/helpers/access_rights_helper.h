// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API AccessRightHelper
{
    Q_DECLARE_TR_FUNCTIONS(AccessRightHelper)

public:
    /** Returns user-friendly name for the given access right */
    static QString name(api::AccessRight accessRight);
};

} // namespace nx::vms::common
