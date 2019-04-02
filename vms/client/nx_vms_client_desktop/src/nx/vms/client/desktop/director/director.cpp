#include "director.h"

#include <utils/common/delayed.h>

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <nx/vms/client/desktop/debug_utils/instruments/debug_info_instrument.h>

namespace nx::vmx::client::desktop {

using namespace vms::client::desktop::ui;

namespace {

int kQuitDelay = 2000; //< 2s.

} // namespace

Director::Director(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

Director::~Director()
{
}

void Director::quit(bool force)
{
    NX_ASSERT(context());

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

std::vector<qint64> Director::getFrameTimePoints()
{
    NX_ASSERT(context());
    return context()->display()->debugInfoInstrument()->getFrameTimePoints();
}

} // namespace nx::vmx::client::desktop