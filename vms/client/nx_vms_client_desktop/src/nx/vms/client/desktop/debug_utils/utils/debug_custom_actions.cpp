// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_custom_actions.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

static std::map<QString, DebugActionHandler> kDebugActions;

} // namespace

void registerDebugAction(const QString& name, DebugActionHandler handler)
{
    NX_ASSERT(!kDebugActions.contains(name));
    kDebugActions[name] = handler;
}

std::map<QString, DebugActionHandler> debugActions()
{
    return kDebugActions;
}

} // namespace nx::vms::client::desktop
