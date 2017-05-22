#include "applauncher_guard.h"

#include <QtCore/QTimerEvent>

#include <client/client_runtime_settings.h>

#include <utils/applauncher_utils.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

// We hope applaucher will crash not so often
static const int kCheckIntervalMs = 60 * 1000;

} // namespace

ApplauncherGuard::ApplauncherGuard(QObject* parent):
    base_type(parent)
{
    // Do not check right now because client is not fully initialized yet
    startTimer(kCheckIntervalMs);
}

void ApplauncherGuard::timerEvent(QTimerEvent* event)
{
    // Make sure activeX plugin will not start applaucher. Checking here because we don't know
    // current client mode in the constructor.
    if (qnRuntime->isActiveXMode())
    {
        killTimer(event->timerId());
        return;
    }

    applauncher::checkOnline();
}

} // namespace desktop
} // namespace client
} // namespace nx
