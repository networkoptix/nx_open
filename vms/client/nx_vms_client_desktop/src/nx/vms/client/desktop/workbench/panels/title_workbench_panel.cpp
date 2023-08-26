// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "title_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>

#include "buttons.h"

namespace nx::vms::client::desktop {

using namespace ui;

TitleWorkbenchPanel::TitleWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    m_widget(new QnMainWindowTitleBarWidget(context(), mainWindow()->view())),
    m_showButton(newShowHideButton(parentGraphicsWidget, context(),
        action::ToggleTitleBarAction)),
    m_yAnimator(new VariantAnimator(this)),
    m_opacityProcessor(new HoverFocusProcessor(parentGraphicsWidget))
{
    m_widget->setStateStore(mainWindow()->titleBar()->stateStore());
    connect(m_widget, &QnMainWindowTitleBarWidget::geometryChanged, this,
        &TitleWorkbenchPanel::updateControlsGeometry);

    const auto toggleTitleBarAction = action(action::ToggleTitleBarAction);
    toggleTitleBarAction->setChecked(settings.state == Qn::PaneState::Opened);
    {
        QTransform transform;
        transform.rotate(-90);
        transform.scale(-1, 1);
        m_showButton->setTransform(transform);
    }

    connect(toggleTitleBarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_opacityProcessor->addTargetItem(m_showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_yAnimator->setTimer(animationTimer());
    m_yAnimator->setTargetObject(m_widget);
    m_yAnimator->setAccessor(new PropertyAccessor("y"));

    updateControlsGeometry();
}

bool TitleWorkbenchPanel::isPinned() const
{
    return true;
}

bool TitleWorkbenchPanel::isOpened() const
{
    return action(action::ToggleTitleBarAction)->isChecked();
}

void TitleWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;

    ensureAnimationAllowed(&animate);

    QScopedValueRollback<bool> guard(m_ignoreClickEvent, true);
    action(action::ToggleTitleBarAction)->setChecked(opened);

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::TitlePanelExpand
        : Animations::Id::TitlePanelCollapse);

    int newY = opened
        ? 0
        : -m_widget->size().height() - kHidePanelOffset;

    if (animate)
        m_yAnimator->animateTo(newY);
    else
        m_widget->setY(newY);

    emit openedChanged(opened, animate);
}

bool TitleWorkbenchPanel::isVisible() const
{
    return !m_widget->isHidden();
}

void TitleWorkbenchPanel::setVisible(bool visible, bool animate)
{
    const bool changed = isVisible() != visible;
    m_widget->setVisible(visible);

    ensureAnimationAllowed(&animate);
    updateOpacity(animate);

    if (changed)
        emit visibleChanged(visible, animate);
}

qreal TitleWorkbenchPanel::opacity() const
{
    return opacityAnimator(m_showButton)->targetValue().toDouble();
}

void TitleWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        opacityAnimator(m_showButton)->pause();
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->start();
    }
    else
    {
        opacityAnimator(m_showButton)->stop();
        m_showButton->setOpacity(opacity);
    }
}

bool TitleWorkbenchPanel::isHovered() const
{
    return m_widget->underMouse() || m_opacityProcessor->isHovered();
}

void TitleWorkbenchPanel::stopAnimations()
{
    if (!m_yAnimator->isRunning())
        return;

    m_yAnimator->stop();
    m_widget->setY(m_yAnimator->targetValue().toInt());
}

QRectF TitleWorkbenchPanel::effectiveGeometry() const
{
    QRect geometry = m_widget->geometry();
    if (m_yAnimator->isRunning())
        geometry.moveTop(m_yAnimator->targetValue().toInt());
    return geometry;
}

void TitleWorkbenchPanel::setPanelSize(qreal /*size*/)
{
    // Nothing to do here. Panel adjusts its size automatically.
}

QRectF TitleWorkbenchPanel::geometry() const
{
    return m_widget->geometry();
}

void TitleWorkbenchPanel::setGeometry(const QRect& geometry)
{
    m_widget->setGeometry(geometry);
}

QSize TitleWorkbenchPanel::sizeHint() const
{
    return m_widget->sizeHint();
}

void TitleWorkbenchPanel::setExpanded(bool value)
{
    if (value)
        m_widget->expand();
    else
        m_widget->collapse();
}

bool TitleWorkbenchPanel::isExpanded() const
{
    return m_widget->isExpanded();
}

void TitleWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();
    QRectF geometry = m_widget->geometry();

    m_showButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_showButton->size().height()) / 2,
        qMax(parentWidgetRect.top(), geometry.bottom())));

    emit geometryChanged();
}

} //namespace nx::vms::client::desktop
