#include "resources_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMenu>

#include <core/resource/resource.h>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui;

namespace {

static const int kResizerWidth = 8;

}

namespace NxUi {

ResourceTreeWorkbenchPanel::ResourceTreeWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    widget(new QnResourceBrowserWidget(nullptr, context())),
    item(new QnMaskedProxyWidget(parentWidget)),
    xAnimator(new VariantAnimator(widget)),
    m_ignoreClickEvent(false),
    m_visible(false),
    m_resizing(false),
    m_updateResizerGeometryLater(false),
    m_resizerWidget(new QnResizerWidget(Qt::Horizontal, parentWidget)),
    m_backgroundItem(new QnControlBackgroundWidget(Qt::LeftEdge, parentWidget)),
    m_showButton(newShowHideButton(parentWidget, context(), action::ToggleTreeAction)),
    m_pinButton(newPinButton(parentWidget, context(), action::PinTreeAction)),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(widget))
{

    widget->setAttribute(Qt::WA_TranslucentBackground);
    connect(widget, &QnResourceBrowserWidget::scrollBarVisibleChanged, this,
        &ResourceTreeWorkbenchPanel::updateControlsGeometry);

    connect(widget, &QnResourceBrowserWidget::selectionChanged, this,
        [this]
        {
            NX_ASSERT(!m_inSelection);

            QScopedValueRollback<bool> guard(m_inSelection, true);
            action(action::SelectionChangeAction)->trigger();
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

    connect(action(action::SelectNewItemAction), &QAction::triggered, this,
        &ResourceTreeWorkbenchPanel::at_selectNewItemAction_triggered);

    QPalette defaultPalette = widget->palette();
    setPaletteColor(widget, QPalette::Window, Qt::transparent);
    setPaletteColor(widget, QPalette::Base, Qt::transparent);
    setPaletteColor(widget->typeComboBox(), QPalette::Base, defaultPalette.color(QPalette::Base));

    m_backgroundItem->setFrameBorders(Qt::RightEdge);
    m_backgroundItem->setZValue(BackgroundItemZOrder);

    item->setWidget(widget);
    widget->installEventFilter(item);
    widget->setToolTipParent(item);
    item->setFocusPolicy(Qt::StrongFocus);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    item->resize(settings.span, 0.0);
    item->setZValue(ContentItemZOrder);
    connect(item, &QnMaskedProxyWidget::paintRectChanged, this,
        &ResourceTreeWorkbenchPanel::updateControlsGeometry);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &ResourceTreeWorkbenchPanel::updateControlsGeometry);
    connect(item, &QGraphicsWidget::geometryChanged, widget,
        &QnResourceBrowserWidget::hideToolTip);

    action(action::ToggleTreeAction)->setChecked(settings.state == Qn::PaneState::Opened);
    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(BackgroundItemZOrder); /*< To make it paint under the tooltip. */
    connect(action(action::ToggleTreeAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    action(action::PinTreeAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    m_pinButton->setFocusProxy(item);
    m_pinButton->setZValue(ControlItemZOrder);
    connect(action(action::PinTreeAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    m_resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    m_resizerWidget->setProperty(Qn::BlockMotionSelection, true);
    m_resizerWidget->setZValue(ResizerItemZOrder);
    connect(m_resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &ResourceTreeWorkbenchPanel::at_resizerWidget_geometryChanged);

    //TODO #vkutin Think how to do it differently.
    //  See another TODO in at_resizerWidget_geometryChanged.
    executeDelayed(
        [this]()
        {
            installEventHandler(mainWindow(), QEvent::Resize, this,
                [this]() { updatePaneWidth(item->size().width()); });
        });

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    m_opacityProcessor->addTargetItem(m_resizerWidget);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->addTargetItem(m_resizerWidget);
    m_hidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);
    connect(menu(), &nx::client::desktop::ui::action::Manager::menuAboutToHide, m_hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            /* Do not auto-hide tree if we have opened context menu. */
            if (!isPinned() && isOpened() && !menu()->isMenuVisible())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(m_showButton);
    m_showingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &ResourceTreeWorkbenchPanel::at_showingProcessor_hoverEntered);

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_pinButton));

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(item, item, Qt::RightEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);
}

bool ResourceTreeWorkbenchPanel::isPinned() const
{
    return action(action::PinTreeAction)->isChecked();
}

bool ResourceTreeWorkbenchPanel::isOpened() const
{
    return action(action::ToggleTreeAction)->isChecked();
}

void ResourceTreeWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace nx::client::desktop::ui::workbench;

    ensureAnimationAllowed(&animate);

    if (!item)
        return;

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(action::ToggleTreeAction)->setChecked(opened);

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::ResourcesPanelExpand
        : Animations::Id::ResourcesPanelCollapse);

    qreal width = item->size().width();
    qreal newX = opened ? 0.0 : - width - kHidePanelOffset;
    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

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

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal ResourceTreeWorkbenchPanel::opacity() const
{
    return opacityAnimator(item)->targetValue().toDouble();
}

void ResourceTreeWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(m_pinButton)->setTargetValue(opacity);
        opacityAnimator(m_backgroundItem)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        m_pinButton->setOpacity(opacity);
        m_backgroundItem->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }

    m_resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

bool ResourceTreeWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF ResourceTreeWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toReal());
    return geometry;
}

void ResourceTreeWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    item->setX(xAnimator->targetValue().toDouble());
}

void ResourceTreeWorkbenchPanel::updateResizerGeometry()
{
    if (m_updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &ResourceTreeWorkbenchPanel::updateResizerGeometry);
        return;
    }

    QRectF treeRect = item->rect();

    QRectF resizerGeometry = QRectF(
        m_parentWidget->mapFromItem(item, treeRect.topRight()),
        m_parentWidget->mapFromItem(item, treeRect.bottomRight()));

    qreal offset = kResizerWidth;
    if (widget->isScrollBarVisible())
        offset += widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent);

    resizerGeometry.moveLeft(resizerGeometry.left() - offset);
    resizerGeometry.setWidth(kResizerWidth);

    if (!qFuzzyEquals(resizerGeometry, m_resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updateResizerGeometryLater, true);

        m_resizerWidget->setGeometry(resizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_resizerWidget->setPos(resizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void ResourceTreeWorkbenchPanel::setProxyUpdatesEnabled(bool updatesEnabled)
{
    base_type::setProxyUpdatesEnabled(updatesEnabled);
    item->setUpdatesEnabled(updatesEnabled);
}

void ResourceTreeWorkbenchPanel::setShowButtonUsed(bool used)
{
    m_showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void ResourceTreeWorkbenchPanel::at_resizerWidget_geometryChanged()
{
    if (m_resizing)
        return;

    QRectF resizerGeometry = m_resizerWidget->geometry();
    if (!resizerGeometry.isValid())
    {
        updateResizerGeometry();
        return;
    }

    qreal x = display()->view()->mapFromGlobal(QCursor::pos()).x();

    /* Calculating real border position. */
    x += 0.5 + kResizerWidth;
    if (widget->isScrollBarVisible())
        x += widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent);

    updatePaneWidth(x);
}

void ResourceTreeWorkbenchPanel::updatePaneWidth(qreal desiredWidth)
{
    const qreal minWidth = item->effectiveSizeHint(Qt::MinimumSize).width();

    //TODO #vkutin Think how to do it differently.
    // At application startup m_controlsWidget has default (not maximized) size, so we cannot use its width here.
    const qreal maxWidth = mainWindow()->width() / 2;

    const qreal targetWidth = qBound(minWidth, desiredWidth, maxWidth);

    QRectF geometry = item->geometry();
    if (!qFuzzyEquals(geometry.width(), targetWidth))
    {
        geometry.setWidth(targetWidth);
        geometry.setLeft(0);
        item->setGeometry(geometry);
    }

    updateResizerGeometry();
}

void ResourceTreeWorkbenchPanel::at_showingProcessor_hoverEntered()
{
    if (!isPinned() && !isOpened())
    {
        setOpened(true, true);

        /* So that the click that may follow won't hide it. */
        setShowButtonUsed(false);
        QTimer::singleShot(NxUi::kButtonInactivityTimeoutMs, this,
            [this]
            {
                setShowButtonUsed(true);
            });
    }

    m_hidingProcessor->forceHoverEnter();
    m_opacityProcessor->forceHoverEnter();
}

void ResourceTreeWorkbenchPanel::at_selectNewItemAction_triggered()
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
}

void ResourceTreeWorkbenchPanel::updateControlsGeometry()
{
    if (m_resizing)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_resizing, true);

    QRectF geometry = item->geometry();
    auto parentWidgetRect = m_parentWidget->rect();
    QRectF paintGeometry = geometry;
    paintGeometry.setLeft(0.0);
    item->setPaintGeometry(paintGeometry);

    m_backgroundItem->setGeometry(paintGeometry);

    m_showButton->setPos(QPointF(
        qMax(parentWidgetRect.left(), geometry.right()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height())/2.0));

    m_pinButton->setPos(QPointF(
        geometry.right() - m_pinButton->size().width() - 1.0,
        geometry.top() + 1.0));

    updateResizerGeometry();

    emit geometryChanged();
}

}