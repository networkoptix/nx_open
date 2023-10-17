// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/left_panel/qml_resource_browser_widget.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include "../workbench_animations.h"
#include "buttons.h"

namespace {

static constexpr int kResizerWidth = 8;
static constexpr int kMinimumWidth = 200;
static constexpr int kMaximumWidth = 480;

} // namespace

namespace nx::vms::client::desktop {

using namespace ui;

ResourceTreeWorkbenchPanel::ResourceTreeWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    widget(new QmlResourceBrowserWidget(windowContext(), parentWidget)),
    m_resizerWidget(new QnResizerWidget(Qt::Horizontal, parentGraphicsWidget)),
    xAnimator(new VariantAnimator(widget)),
    m_backgroundItem(new QnControlBackgroundWidget(parentGraphicsWidget)),
    m_showButton(newShowHideButton(parentGraphicsWidget, windowContext(), menu::ToggleTreeAction)),
    m_opacityAnimatorGroup(new AnimatorGroup(widget))
{
    const auto updateGeometries =
        [this]
        {
            if (m_resizeInProgress)
                return;

            const QScopedValueRollback<bool> guard(m_resizeInProgress, true);
            updateResizerGeometry();
            updateControlsGeometry();
        };

    installEventHandler(widget, {QEvent::Resize, QEvent::Move}, this, updateGeometries,
        Qt::QueuedConnection);

    m_backgroundItem->setFrameBorders(Qt::RightEdge);
    m_backgroundItem->setZValue(BackgroundItemZOrder);

    action(menu::ToggleTreeAction)->setChecked(settings.state == Qn::PaneState::Opened);

    connect(action(menu::ToggleTreeAction), &QAction::toggled,
        this, [this](bool opened) { if (!m_blockAction) setOpened(opened); });

    m_resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    m_resizerWidget->setProperty(Qn::BlockMotionSelection, true);
    m_resizerWidget->setZValue(ResizerItemZOrder);
    m_showButton->setZValue(ButtonItemZOrder);
    connect(m_resizerWidget, &QGraphicsWidget::geometryChanged, this,
        [this]
        {
            if (m_resizeInProgress)
                return;

            if (!m_resizerWidget->geometry().isValid())
            {
                updateResizerGeometry();
                return;
            }

            if (!m_resizerWidget->isBeingMoved())
                return;

            const qreal x = mainWindow()->view()->mapFromGlobal(QCursor::pos()).x();
            updatePaneWidth(x);
            updateControlsGeometry();
            updateResizerGeometry();
        });

    executeDelayed(
        [this]()
        {
            installEventHandler(mainWindowWidget(), QEvent::Resize, this,
                [this]() { updatePaneWidth(widget->size().width()); });
        });

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(widget);
    xAnimator->setAccessor(new PropertyAccessor("position"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));

    connect(m_backgroundItem, &QGraphicsWidget::opacityChanged, this,
        [this]()
        {
            const auto opacity = m_backgroundItem->opacity();
            widget->setOpacity(opacity);
            widget->setVisible(opacity > 0.0);
        });
}

void ResourceTreeWorkbenchPanel::setYAndHeight(int y, int height)
{
    QRect geometry = widget->geometry();
    geometry.setY(y);
    geometry.setHeight(height);
    widget->setGeometry(geometry);
}

QRectF ResourceTreeWorkbenchPanel::geometry() const
{
    return widget->geometry();
}

bool ResourceTreeWorkbenchPanel::isFocused() const
{
    return widget == qApp->focusWidget();
}

void ResourceTreeWorkbenchPanel::setPanelSize(qreal panelSize)
{
    widget->resize((int) panelSize, widget->height());
    if (!isOpened())
        widget->setPosition(-widget->width());
}

bool ResourceTreeWorkbenchPanel::isPinned() const
{
    return true;
}

bool ResourceTreeWorkbenchPanel::isOpened() const
{
    return action(menu::ToggleTreeAction)->isChecked();
}

void ResourceTreeWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;

    ensureAnimationAllowed(&animate);

    auto toggleAction = action(menu::ToggleTreeAction);
    m_blockAction = true;
    toggleAction->setChecked(opened);
    m_blockAction = false;

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::ResourcesPanelExpand
        : Animations::Id::ResourcesPanelCollapse);

    const qreal width = widget->size().width();
    const qreal newX = opened ? 0 : -width - kHidePanelOffset;
    if (animate)
        xAnimator->animateTo(newX);
    else
        widget->move((int) newX, widget->y());

    m_resizerWidget->setEnabled(opened);

    emit openedChanged(opened, animate);
}

bool ResourceTreeWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void ResourceTreeWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;
    widget->setVisible(visible);

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal ResourceTreeWorkbenchPanel::opacity() const
{
    return opacityAnimator(m_backgroundItem)->targetValue().toDouble();
}

void ResourceTreeWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(m_backgroundItem)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        m_backgroundItem->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }

    m_resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

bool ResourceTreeWorkbenchPanel::isHovered() const
{
    return false;
}

QRectF ResourceTreeWorkbenchPanel::effectiveGeometry() const
{
    QRect geometry = widget->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toInt());
    return geometry;
}

void ResourceTreeWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    widget->move(xAnimator->targetValue().toInt(), widget->y());
}

void ResourceTreeWorkbenchPanel::updateResizerGeometry()
{
    if (m_updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &ResourceTreeWorkbenchPanel::updateResizerGeometry);
        return;
    }

    QRectF treeRect = widget->rect();
    QRectF resizerGeometry = QRectF(treeRect.topRight(), treeRect.bottomRight());
    resizerGeometry.setWidth(kResizerWidth);

    if (!qFuzzyEquals(resizerGeometry, m_resizerWidget->geometry()))
    {
        const QScopedValueRollback<bool> guard(m_updateResizerGeometryLater, true);

        m_resizerWidget->setGeometry(resizerGeometry);

        // This one is needed here as we are in a handler and thus geometry change does not adjust
        // position.
        m_resizerWidget->setPos(resizerGeometry.topLeft());
    }
}

void ResourceTreeWorkbenchPanel::setShowButtonUsed(bool used)
{
    m_showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void ResourceTreeWorkbenchPanel::updatePaneWidth(qreal desiredWidth)
{
    if (xAnimator->isRunning())
        return;

    const int targetWidth = qBound(kMinimumWidth, (int) desiredWidth, kMaximumWidth);
    widget->resize(targetWidth, widget->height());
}

void ResourceTreeWorkbenchPanel::updateControlsGeometry()
{
    const QRectF geometry = widget->geometry();

    QRectF paintGeometry = geometry;
    paintGeometry.setLeft(0.0);

    QRectF backgroundItemGeometry =
        paintGeometry.adjusted(0.0, 0.0, m_backgroundItem->frameWidth(), 0.0);
#if defined(Q_OS_WIN)
    // Windows specific workaround, looks neat at any DPI, otherwise doesn't.
    backgroundItemGeometry.adjust(0.0, 0.0, 0.5, 0.0);
#endif
    m_backgroundItem->setGeometry(backgroundItemGeometry);

    const QRectF parentWidgetRect = m_parentWidget->rect();
    m_showButton->setPos(QPointF(
        qMax(parentWidgetRect.left(), geometry.right()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height())/2.0));

    emit geometryChanged();
}

} // namespace nx::vms::client::desktop
