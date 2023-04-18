// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_types.h"

#include <nx/utils/log/to_string.h>

namespace nx::vms::api {

void PrintTo(GlobalPermission value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(GlobalPermissions value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(AccessRight value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(AccessRights value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

void PrintTo(SpecialResourceGroup value, std::ostream* os)
{
    *os << nx::toString(value).toStdString();
}

} // namespace nx::vms::api
