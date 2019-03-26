#include "director.h"

#include <utils/common/delayed.h>

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vmx::client::desktop {

using namespace vms::client::desktop::ui;

namespace {

int kQuitDelay = 2000; //< 2s.

} // namespace

void Director::quit(bool force)
{
    if (force)
    {
        executeDelayedParented(
            [this] { context()->action(action::DelayedForcedExitAction)->trigger(); },
            kQuitDelay, this);
    }
    else
    {
        executeDelayedParented(
            [this] { context()->action(action::ExitAction)->trigger(); },
            kQuitDelay, this);
    }
}

Director::Director(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

Director::~Director()
{
}

} // namespace nx::vmx::client::desktop