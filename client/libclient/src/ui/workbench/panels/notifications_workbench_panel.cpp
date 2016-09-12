#include "notifications_workbench_panel.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
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
    Controls
};

}

namespace NxUi {

NotificationsWorkbenchPanel::NotificationsWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    backgroundItem(new QnControlBackgroundWidget(Qn::RightBorder, parentWidget)),
    item(new QnNotificationsCollectionWidget(parentWidget, 0, context())),
    pinButton(NxUi::newPinButton(parentWidget, context(),
        action(QnActions::PinNotificationsAction))),
    showButton(NxUi::newBlinkingShowHideButton(parentWidget, context(),
        action(QnActions::ToggleNotificationsAction))),
    xAnimator(new VariantAnimator(this)),

    m_ignoreClickEvent(false),
    m_visible(false),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
{
    /* Notifications panel. */
    backgroundItem->setFrameBorders(Qt::LeftEdge);
    backgroundItem->setZValue(ZOrder::BackgroundItem);

    item->setZValue(ZOrder::Item);
    item->setProperty(Qn::NoHandScrollOver, true);
    setHelpTopic(item, Qn::MainWindow_Notifications_Help);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &NotificationsWorkbenchPanel::at_item_geometryChanged);

    action(QnActions::PinNotificationsAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    pinButton->setFocusProxy(item);
    pinButton->setZValue(ZOrder::Controls);
    connect(action(QnActions::PinNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    action(QnActions::ToggleNotificationsAction)->setChecked(settings.state == Qn::PaneState::Opened);
    showButton->setTransform(QTransform::fromScale(-1, 1));
    showButton->setFocusProxy(item);
    showButton->setZValue(ZOrder::Controls);
    setHelpTopic(showButton, Qn::MainWindow_Pin_Help);
    item->setBlinker(showButton);
    connect(action(QnActions::ToggleNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(showButton);
    m_hidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(showButton);
    m_showingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered);

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
    new QnEdgeShadowWidget(item, Qt::LeftEdge, NxUi::kShadowThickness);
}

bool NotificationsWorkbenchPanel::isPinned() const
{
    return action(QnActions::PinNotificationsAction)->isChecked();
}

bool NotificationsWorkbenchPanel::isOpened() const
{
    return action(QnActions::ToggleNotificationsAction)->isChecked();
}

void NotificationsWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleNotificationsAction)->setChecked(opened);

    xAnimator->stop();
    if (opened)
        xAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    else
        xAnimator->setEasingCurve(QEasingCurve::OutCubic);
    xAnimator->setTimeLimit(opened ? kShowAnimationDurationMs : kHideAnimationDurationMs);

    qreal width = item->size().width();
    qreal newX = m_parentWidgetRect.right()
        + (opened ? -width : 1.0 /* Just in case. */);

    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

    emit openedChanged(opened);
}

bool NotificationsWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void NotificationsWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible);
}

qreal NotificationsWorkbenchPanel::opacity() const
{
    return opacityAnimator(item)->targetValue().toDouble();
}

void NotificationsWorkbenchPanel::setOpacity(qreal opacity, bool animate)
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
}

bool NotificationsWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

void NotificationsWorkbenchPanel::setShowButtonUsed(bool used)
{
    showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void NotificationsWorkbenchPanel::at_item_geometryChanged()
{
    QRectF headerGeometry = m_parentWidget->mapRectFromItem(item, item->headerGeometry());
    QRectF backgroundGeometry = m_parentWidget->mapRectFromItem(item, item->visibleGeometry());

    QRectF paintGeometry = item->geometry();
    backgroundItem->setGeometry(paintGeometry);
    showButton->setPos(QPointF(
        qMin(m_parentWidgetRect.right(), paintGeometry.left()),
        (m_parentWidgetRect.top() + m_parentWidgetRect.bottom() - showButton->size().height()) / 2
    ));
    pinButton->setPos(headerGeometry.topLeft() + QPointF(1.0, 1.0));

    emit geometryChanged();
}

void NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered()
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

} //namespace NxUi
