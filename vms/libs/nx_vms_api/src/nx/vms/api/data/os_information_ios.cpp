// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_information.h"

#include <QtCore/QString>

namespace nx::vms::api {

QString OsInformation::currentSystemRuntime()
{
    return "iOS";
}

} // namespace nx::vms::api
