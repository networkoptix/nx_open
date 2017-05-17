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

}

ApplauncherGuard::ApplauncherGuard(QObject* parent):
    base_type(parent)
{
    // Do not check right now because client is not fully initialized yet
    startTimer(kCheckIntervalMs);
}

void ApplauncherGuard::timerEvent(QTimerEvent* event)
{
    base_type::timerEvent(event);

    // Make sure activeX plugin will not start applaucher.
    if (qnRuntime->isActiveXMode())
        return;

    applauncher::checkOnline();
}

} // namespace desktop
} // namespace client
} // namespace nx
