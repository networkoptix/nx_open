// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rest_error_strings.h"

namespace nx::vms::client::desktop {

QString RestErrorStrings::getPersistentStringPart(int errorId)
{
    return tr("Error code: %1").arg(errorId);
}

} // namespace nx::vms::client::desktop
