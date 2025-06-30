// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>

namespace nx::vms::client::desktop {

enum class SessionRefreshFlag
{
    sessionAware = 1 << 0,
    stayOnTop = 1 << 1,
};

Q_DECLARE_FLAGS(SessionRefreshFlags, SessionRefreshFlag)

} // namespace nx::vms::client::desktop
