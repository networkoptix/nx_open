#include "debug_custom_actions.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

static QMap<QString, DebugActionHandler> kDebugActions;

} // namespace

void registerDebugAction(const QString& name, DebugActionHandler handler)
{
    NX_ASSERT(!kDebugActions.contains(name));
    kDebugActions[name] = handler;
}

QMap<QString, DebugActionHandler> debugActions()
{
    return kDebugActions;
}

} // namespace nx::vms::client::desktop
