// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_resources_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>

#include <QtWidgets/QApplication>
#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#include <core/resource/resource.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/processors/hover_processor.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include "../workbench_animations.h"
#include "buttons.h"

namespace {

static const int kResizerWidth = 8;

} // namespace

namespace nx::vms::client::desktop {
namespace deprecated {

using namespace ui;

ResourceTreeWorkbenchPanel::ResourceTreeWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    widget(new QnResourceBrowserWidget(context(), parentWidget)),
    m_resizerWidget(new QnResizerWidget(Qt::Horizontal, parentGraphicsWidget)),
    xAnimator(new VariantAnimator(widget)),
    m_backgroundItem(new QnControlBackgroundWidget(parentGraphicsWidget)),
    m_showButton(newShowHideButton(parentGraphicsWidget, context(), action::ToggleTreeAction)),
    m_opacityAnimatorGroup(new AnimatorGroup(widget))
{
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    widget->setMouseTracking(true);

    connect(widget, &QnResourceBrowserWidget::scrollBarVisibleChanged, this,
        [this]
        {
            updateResizerGeometry();
            updateControlsGeometry();
        });

    connect(widget, &QnResourceBrowserWidget::selectionChanged, this,
        [this]
        {
            NX_ASSERT(!m_inSelection);

            QScopedValueRollback<bool> guard(m_inSelection, true);
            menu()->trigger(action::SelectionChangeAction);
        });
    connect(action(action::SelectionChangeAction), &QAction::triggered, this,
        [this]
        {
            if (m_inSelection)
                return;

            // Blocking signals to avoid consequent SelectionChanged action call
            QSignalBlocker blocker(widget);
            widget->clearSelection();
        });

    connect(widget, &QnResourceBrowserWidget::geometryChanged, this,
        [this]
        {
            if (!m_resizeInProgress)
            {
                QScopedValueRollback<bool> guard(m_resizeInProgress, true);
                updateResizerGeometry();
                updateControlsGeometry();
            }
        });

    connect(widget, &QnResourceBrowserWidget::geometryChanged,
        widget, &QnResourceBrowserWidget::hideToolTip);

    connect(action(action::SelectNewItemAction), &QAction::triggered, this,
        [this]
        {
            NX_ASSERT(!m_inSelection);
            QScopedValueRollback<bool> guard(m_inSelection, true);

            // Blocking signals to avoid SelectionChanged action call
            QSignalBlocker blocker(widget);

            const auto parameters = menu()->currentParameters(sender());
            if (parameters.hasArgument(Qn::UuidRole))
            {
                const auto id = parameters.argument(Qn::UuidRole).value<QnUuid>();
                widget->selectNodeByUuid(id);
            }
            else if (const auto resource = parameters.resource())
            {
                widget->selectResource(resource);
            }
        });

    QPalette defaultPalette = widget->palette();
    setPaletteColor(widget, QPalette::Window, Qt::transparent);
    setPaletteColor(widget, QPalette::Base, Qt::transparent);

    m_backgroundItem->setFrameBorders(Qt::RightEdge);
    m_backgroundItem->setZValue(BackgroundItemZOrder);

    action(action::ToggleTreeAction)->setChecked(settings.state == Qn::PaneState::Opened);

    connect(action(action::ToggleTreeAction), &QAction::toggled,
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

            qreal x = display()->view()->mapFromGlobal(QCursor::pos()).x();

            /* Calculating real border position. */
            if (widget->isScrollBarVisible())
                x += widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent);

            updatePaneWidth(x);
            updateControlsGeometry();
            updateResizerGeometry();
        });

    //TODO #vkutin Think how to do it differently.
    //  See another TODO in at_resizerWidget_geometryChanged.
    executeDelayed(
        [this]()
        {
            installEventHandler(mainWindowWidget(), QEvent::Resize, this,
                [this]() { updatePaneWidth(widget->size().width()); });
        });

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(widget);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));
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
    return (widget == qApp->focusWidget())
        || (widget && (widget->treeView() == qApp->focusWidget()));
}

void ResourceTreeWorkbenchPanel::setPanelSize(qreal panelSize)
{
    widget->resize(static_cast<int>(panelSize), static_cast<int>(widget->geometry().height()));
    if (!isOpened())
        widget->setX(-widget->width());
}

bool ResourceTreeWorkbenchPanel::isPinned() const
{
    return true;
}

bool ResourceTreeWorkbenchPanel::isOpened() const
{
    return action(action::ToggleTreeAction)->isChecked();
}

void ResourceTreeWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;

    ensureAnimationAllowed(&animate);

    auto toggleAction = action(action::ToggleTreeAction);
    m_blockAction = true;
    toggleAction->setChecked(opened);
    m_blockAction = false;

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::ResourcesPanelExpand
        : Animations::Id::ResourcesPanelCollapse);

    int width = widget->size().width();
    int newX = opened ? 0 : - width - static_cast<int>(kHidePanelOffset);
    if (animate)
        xAnimator->animateTo(newX);
    else
        widget->move(static_cast<int>(newX), widget->y());

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
        QScopedValueRollback<bool> guard(m_updateResizerGeometryLater, true);

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
    const int minWidth = widget->sizeHint().width();

    //TODO #vkutin Think how to do it differently.
    // At application startup m_controlsWidget has default (not maximized) size, so we cannot use its width here.
    const int maxWidth = mainWindowWidget()->width() / 2;

    const int targetWidth = qBound(minWidth, static_cast<int>(desiredWidth), maxWidth);

    if (!xAnimator->isRunning())
    {
        QRect geometry = widget->geometry();
        if (geometry.width() != targetWidth)
        {
            geometry.setWidth(targetWidth);
            widget->setGeometry(geometry);
        }
    }
}

void ResourceTreeWorkbenchPanel::updateControlsGeometry()
{
    QRectF geometry = widget->geometry();

    QRectF paintGeometry = geometry;
    paintGeometry.setLeft(0.0);

    auto backgroundItemGeometry =
        paintGeometry.adjusted(0.0, 0.0, m_backgroundItem->frameWidth(), 0.0);
    #if defined(Q_OS_WIN)
        // Windows specific workaround, looks neat at any DPI, otherwise doesn't.
        backgroundItemGeometry.adjust(0.0, 0.0, 0.5, 0.0);
    #endif
    m_backgroundItem->setGeometry(backgroundItemGeometry);

    QRectF parentWidgetRect = m_parentWidget->rect();
    m_showButton->setPos(QPointF(
        qMax(parentWidgetRect.left(), geometry.right()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height())/2.0));

    emit geometryChanged();
}

} // namespace deprecated
} // namespace nx::vms::client::desktop
