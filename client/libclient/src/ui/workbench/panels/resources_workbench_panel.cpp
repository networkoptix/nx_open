#include "resources_workbench_panel.h"

#include <ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

namespace {

static const int kShowAnimationDurationMs = 300;
static const int kHideAnimationDurationMs = 200;

enum ZOrder
{
    BackgroundItem,
    Item,
    Resizer,
    Controls
};

}

namespace NxUi {

ResourceTreeWorkbenchPanel::ResourceTreeWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    widget(new QnResourceBrowserWidget(nullptr, context())),
    resizerWidget(new QnResizerWidget(Qt::Horizontal, parentWidget)),
    item(new QnMaskedProxyWidget(parentWidget)),
    backgroundItem(new QnControlBackgroundWidget(Qn::LeftBorder, parentWidget)),
    showButton(newShowHideButton(parentWidget, context(), action(QnActions::ToggleTreeAction))),
    pinButton(newPinButton(parentWidget, context(), action(QnActions::PinTreeAction))),
    xAnimator(new VariantAnimator(widget)),

    m_ignoreClickEvent(false),
    m_ignoreResizerGeometryChanges(false),
    m_updateResizerGeometryLater(false),
    m_visible(false),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(widget))
{
    widget->setAttribute(Qt::WA_TranslucentBackground);
    connect(widget, &QnResourceBrowserWidget::selectionChanged,
        action(QnActions::SelectionChangeAction), &QAction::trigger);

    QPalette defaultPalette = widget->palette();
    setPaletteColor(widget, QPalette::Window, Qt::transparent);
    setPaletteColor(widget, QPalette::Base, Qt::transparent);
    setPaletteColor(widget->typeComboBox(), QPalette::Base, defaultPalette.color(QPalette::Base));

    backgroundItem->setFrameBorders(Qt::RightEdge);
    backgroundItem->setZValue(ZOrder::BackgroundItem);

    item->setWidget(widget);
    widget->installEventFilter(item);
    widget->setToolTipParent(item);
    item->setFocusPolicy(Qt::StrongFocus);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->resize(settings.span, 0.0);
    item->setZValue(ZOrder::Item);

    action(QnActions::ToggleTreeAction)->setChecked(settings.state == Qn::PaneState::Opened);
    showButton->setFocusProxy(item);
    showButton->setZValue(ZOrder::Controls);
    connect(action(QnActions::ToggleTreeAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    action(QnActions::PinTreeAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    pinButton->setFocusProxy(item);
    pinButton->setZValue(ZOrder::Controls);

    resizerWidget->setProperty(Qn::NoHandScrollOver, true);
    connect(resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &ResourceTreeWorkbenchPanel::at_resizerWidget_geometryChanged, Qt::QueuedConnection);
    resizerWidget->setZValue(ZOrder::Resizer);

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(showButton);
    m_hidingProcessor->addTargetItem(resizerWidget);
    m_hidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);
    connect(menu(), &QnActionManager::menuAboutToHide, m_hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            /* Do not auto-hide tree if we have opened context menu. */
            if (!isPinned() && isOpened() && !menu()->isMenuVisible())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(showButton);
    m_showingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &ResourceTreeWorkbenchPanel::at_showingProcessor_hoverEntered);

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));
    xAnimator->setSpeed(1.0);

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem)); /* Speed of 1.0 is OK here. */
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(showButton));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(pinButton));

    /* Create a shadow: */
    new QnEdgeShadowWidget(item, Qt::RightEdge, NxUi::kShadowThickness);
}

bool ResourceTreeWorkbenchPanel::isPinned() const
{
    return action(QnActions::PinTreeAction)->isChecked();
}

bool ResourceTreeWorkbenchPanel::isOpened() const
{
    return action(QnActions::ToggleTreeAction)->isChecked();
}

void ResourceTreeWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (!item)
        return;

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleTreeAction)->setChecked(opened);

    xAnimator->stop();
    if (opened)
        xAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    else
        xAnimator->setEasingCurve(QEasingCurve::OutCubic);

    qreal width = item->size().width();
    xAnimator->setTimeLimit(opened ? kShowAnimationDurationMs : kHideAnimationDurationMs);

    qreal newX = opened ? 0.0 : - width - 1.0 /* Just in case. */;
    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

    resizerWidget->setEnabled(opened);
    emit openedChanged(opened);
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
        emit visibleChanged(visible);
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
        opacityAnimator(pinButton)->setTargetValue(opacity);
        opacityAnimator(backgroundItem)->setTargetValue(opacity);
        opacityAnimator(showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        pinButton->setOpacity(opacity);
        backgroundItem->setOpacity(opacity);
        showButton->setOpacity(opacity);
    }

    resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

bool ResourceTreeWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

void ResourceTreeWorkbenchPanel::updateResizerGeometry()
{
    if (m_updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &ResourceTreeWorkbenchPanel::updateResizerGeometry);
        return;
    }

    QRectF treeRect = item->rect();

    QRectF treeResizerGeometry = QRectF(
        m_parentWidget->mapFromItem(item, treeRect.topRight()),
        m_parentWidget->mapFromItem(item, treeRect.bottomRight()));

    treeResizerGeometry.moveTo(treeResizerGeometry.topRight());
    treeResizerGeometry.setWidth(8);

    if (!qFuzzyEquals(treeResizerGeometry, resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updateResizerGeometryLater, true);

        resizerWidget->setGeometry(treeResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        resizerWidget->setPos(treeResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void ResourceTreeWorkbenchPanel::setProxyUpdatesEnabled(bool updatesEnabled)
{
    base_type::setProxyUpdatesEnabled(updatesEnabled);
    item->setUpdatesEnabled(updatesEnabled);
}

void ResourceTreeWorkbenchPanel::setShowButtonUsed(bool used)
{
    showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void ResourceTreeWorkbenchPanel::at_resizerWidget_geometryChanged()
{
    if (m_ignoreResizerGeometryChanges)
        return;

    QRectF resizerGeometry = resizerWidget->geometry();
    if (!resizerGeometry.isValid())
    {
        updateResizerGeometry();
        return;
    }

    QRectF treeGeometry = item->geometry();

    qreal targetWidth = resizerWidget->geometry().left() - treeGeometry.left();
    qreal minWidth = item->effectiveSizeHint(Qt::MinimumSize).width();

    //TODO #vkutin Think how to do it differently.
    // At application startup m_controlsWidget has default (not maximized) size, so we cannot use its width here.
    qreal maxWidth = mainWindow()->width() / 2;

    targetWidth = qBound(minWidth, targetWidth, maxWidth);

    if (!qFuzzyCompare(treeGeometry.width(), targetWidth))
    {
        treeGeometry.setWidth(targetWidth);
        treeGeometry.setLeft(0);

        QN_SCOPED_VALUE_ROLLBACK(&m_ignoreResizerGeometryChanges, true);
        widget->resize(targetWidth, widget->height());
        item->setPaintGeometry(treeGeometry);
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

}