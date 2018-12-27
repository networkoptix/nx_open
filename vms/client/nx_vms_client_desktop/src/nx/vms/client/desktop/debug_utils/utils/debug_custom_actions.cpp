#include "debug_custom_actions.h"

namespace nx::vms::client::desktop {

namespace {

static QMap<QString, std::function<void(void)>> kDebugActions;

} // namespace

void registerDebugAction(const QString& name, std::function<void(void)> handler)
{
    NX_ASSERT(!kDebugActions.contains(name));
    kDebugActions[name] = handler;
}


QMap<QString, std::function<void(void)>> debugActions()
{
    return kDebugActions;
}

} // namespace nx::vms::client::desktop
