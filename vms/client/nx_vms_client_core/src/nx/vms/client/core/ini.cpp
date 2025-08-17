// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

#include <QtCore/QString>

namespace nx::vms::client::core {

Ini& ini()
{
    static Ini ini;
    return ini;
}

bool Ini::isAutoCloudHostDeductionMode() const
{
    // Allow to connect to any cloud host in demo mode if cloud host is not passed explicitly.
    QString cloudHost(this->cloudHost);
    return cloudHost == "auto" || cloudHost.isEmpty();
}

} // nx::vms::client::core
