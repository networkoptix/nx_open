#include "abstract_workbench_panel.h"

#include <client/client_settings.h>

#include <ui/animation/animation_timer.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_ui_globals.h>

namespace NxUi {

AbstractWorkbenchPanel::AbstractWorkbenchPanel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(display(), &QnWorkbenchDisplay::viewportGrabbed, this,
        [this]
        {
            setProxyUpdatesEnabled(false);
        });
    connect(display(), &QnWorkbenchDisplay::viewportUngrabbed, this,
        [this]
        {
            setProxyUpdatesEnabled(true);
        });
}

void AbstractWorkbenchPanel::updateOpacity(bool animate)
{
    const qreal opacity = isVisible() ? NxUi::kOpaque : NxUi::kHidden;
    setOpacity(opacity, animate);
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

void AbstractWorkbenchPanel::setProxyUpdatesEnabled(bool updatesEnabled)
{
}

} //namespace NxUi
