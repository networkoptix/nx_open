// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::common {

struct ResourceDescriptor
{
    QnUuid id;
    QString path;
};

} // namespace nx::vms::common
