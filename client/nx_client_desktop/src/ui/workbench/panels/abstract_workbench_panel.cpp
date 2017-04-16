#include "abstract_workbench_panel.h"

#include <client/client_settings.h>

#include <ui/animation/animation_timer.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_ui_globals.h>

namespace NxUi {

AbstractWorkbenchPanel::AbstractWorkbenchPanel(
    const QnPaneSettings& /*settings*/, /*< Possibly will be used later. */
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_parentWidget(parentWidget),
    m_masterOpacity(NxUi::kOpaque)
{
    NX_ASSERT(m_parentWidget);

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

void AbstractWorkbenchPanel::setProxyUpdatesEnabled(bool /* updatesEnabled */ )
{
    /* This method may be overridden in panels to disable and enable masked widgets. */
}

qreal AbstractWorkbenchPanel::masterOpacity() const
{
    return m_masterOpacity;
}

void AbstractWorkbenchPanel::setMasterOpacity(qreal value)
{
    value = qBound(NxUi::kHidden, value, NxUi::kOpaque);
    if (qFuzzyIsNull(m_masterOpacity - value))
        return;

    m_masterOpacity = value;
    updateOpacity(false);
}

} //namespace NxUi
