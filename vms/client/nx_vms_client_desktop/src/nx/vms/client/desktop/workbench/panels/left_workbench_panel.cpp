// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "left_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#include <core/resource/resource.h>
#include <nx/build_info.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/left_panel/left_panel_widget.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include "../workbench_animations.h"
#include "buttons.h"

namespace nx::vms::client::desktop {

using namespace ui;

LeftWorkbenchPanel::LeftWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    widget(new LeftPanelWidget(windowContext(), parentWidget)),
    xAnimator(new VariantAnimator(widget)),
    m_backgroundItem(new QnControlBackgroundWidget(parentGraphicsWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(widget))
{
    installEventHandler(widget, {QEvent::Resize, QEvent::Move}, this,
        [this](QObject* /*watched*/, QEvent* event)
        {
            if (event->type() == QEvent::Resize && !isOpened())
            {
                // Update position of hidden or sliding away panel.
                setOpened(/*opened*/ false, /*animate*/ xAnimator->isRunning());
            }
            updateControlsGeometry();
        });

    m_backgroundItem->setFrameBorders({});
    m_backgroundItem->setZValue(BackgroundItemZOrder);

    action(menu::ToggleLeftPanelAction)->setChecked(settings.state == Qn::PaneState::Opened);

    connect(action(menu::ToggleLeftPanelAction), &QAction::toggled, this,
        [this](bool opened)
        {
            if (!m_blockAction)
                setOpened(opened);
        });

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(widget);
    xAnimator->setAccessor(new PropertyAccessor("position"));
    connect(xAnimator, &AbstractAnimator::finished, this, &LeftWorkbenchPanel::updateMaximumWidth);

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_backgroundItem));

    connect(m_backgroundItem, &QGraphicsWidget::opacityChanged, this,
        [this]()
        {
            const auto opacity = m_backgroundItem->opacity();
            widget->setOpacity(opacity);
            widget->setVisible(opacity > 0.0);
        });
}

void LeftWorkbenchPanel::setYAndHeight(int y, int height)
{
    widget->move(widget->x(), y);
    widget->setHeight(height);
}

QRectF LeftWorkbenchPanel::geometry() const
{
    return widget->geometry();
}

bool LeftWorkbenchPanel::isFocused() const
{
    return widget == qApp->focusWidget();
}

bool LeftWorkbenchPanel::isPinned() const
{
    return true;
}

bool LeftWorkbenchPanel::isOpened() const
{
    return action(menu::ToggleLeftPanelAction)->isChecked();
}

void LeftWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;
    ensureAnimationAllowed(&animate);

    auto toggleAction = action(menu::ToggleLeftPanelAction);
    m_blockAction = true;
    toggleAction->setChecked(opened);
    m_blockAction = false;

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::ResourcesPanelExpand
        : Animations::Id::ResourcesPanelCollapse);

    const qreal newX = opened ? 0 : -effectiveWidth() - static_cast<int>(kHidePanelOffset);
    if (animate)
        xAnimator->animateTo(newX);
    else
        widget->move(static_cast<int>(newX), widget->y());

    widget->setResizeEnabled(opened);
    updateMaximumWidth();

    emit openedChanged(opened, animate);
}

bool LeftWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void LeftWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal LeftWorkbenchPanel::opacity() const
{
    return opacityAnimator(m_backgroundItem)->targetValue().toDouble();
}

void LeftWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(m_backgroundItem)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        m_backgroundItem->setOpacity(opacity);
    }
}

bool LeftWorkbenchPanel::isHovered() const
{
    return false;
}

QRectF LeftWorkbenchPanel::effectiveGeometry() const
{
    QRect geometry = widget->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toInt());

    geometry.setWidth(effectiveWidth());

    return geometry;
}

void LeftWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    widget->move(xAnimator->targetValue().toInt(), widget->y());
}

void LeftWorkbenchPanel::updateControlsGeometry()
{
    QRectF geometry = widget->geometry();
    geometry.setWidth(effectiveWidth());
    geometry.setLeft(0.0);

    if (nx::build_info::isWindows())
    {
        // Windows specific workaround, looks neat at any DPI, otherwise doesn't.
        geometry.adjust(0.0, 0.0, 0.5, 0.0);
    }

    m_backgroundItem->setGeometry(geometry);

    if (auto button = widget->openCloseButton())
    {
        const auto parentWidgetRect = m_parentWidget->rect();
        QmlProperty<qreal>(button, "y").setValue(-widget->y() +
            (parentWidgetRect.top() + parentWidgetRect.bottom() - button->height()) / 2.0);
    }

    emit geometryChanged();
}

void LeftWorkbenchPanel::setMaximumAllowedWidth(qreal value)
{
    m_maximumAllowedWidth = value;
    updateMaximumWidth();
}

void LeftWorkbenchPanel::updateMaximumWidth()
{
    if (isOpened() && !xAnimator->isRunning())
        widget->setMaximumAllowedWidth(m_maximumAllowedWidth);
}

int LeftWorkbenchPanel::effectiveWidth() const
{
    QRect geometry = widget->geometry();
    const auto button = widget->openCloseButton();
    return geometry.width() - (button ? button->width() : 0);
}

menu::ActionScope LeftWorkbenchPanel::currentScope() const
{
    return widget->currentScope();
}

menu::Parameters LeftWorkbenchPanel::currentParameters(menu::ActionScope scope) const
{
    return widget->currentParameters(scope);
}

} // namespace nx::vms::client::desktop
