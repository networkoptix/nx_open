#include "abstract_workbench_panel.h"

#include <client/client_settings.h>

#include <ui/animation/animation_timer.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>

namespace NxUi {

AbstractWorkbenchPanel::AbstractWorkbenchPanel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{

}

void AbstractWorkbenchPanel::ensureAnimationAllowed(bool* animate)
{
    if (animate && qnSettings->lightMode().testFlag(Qn::LightModeNoAnimation))
        *animate = false;
}

AnimationTimer* AbstractWorkbenchPanel::animationTimer() const
{
    return display()->instrumentManager()->animationTimer();
}

} //namespace NxUi
