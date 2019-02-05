#include "applauncher_guard.h"

#include <QtCore/QTimerEvent>

#include <client/client_runtime_settings.h>

#include <utils/applauncher_utils.h>
#include <common/common_module.h>

namespace nx::vms::client::desktop {

namespace {

// We hope applaucher will crash not so often
static const int kCheckIntervalMs = 60 * 1000;

} // namespace

ApplauncherGuard::ApplauncherGuard(
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(parent),
    QnCommonModuleAware(commonModule)
{
    // Do not check right now because client is not fully initialized yet
    startTimer(kCheckIntervalMs);
}

void ApplauncherGuard::timerEvent(QTimerEvent* event)
{
    // Make sure activeX plugin will not start applaucher. Checking here because we don't know
    // current client mode in the constructor.
    if (qnRuntime->isAcsMode())
    {
        killTimer(event->timerId());
        return;
    }

    applauncher::api::checkOnline(commonModule()->engineVersion());
}

} // namespace nx::vms::client::desktop
