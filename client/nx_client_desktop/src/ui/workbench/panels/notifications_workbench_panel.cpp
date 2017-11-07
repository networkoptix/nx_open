#include "notifications_workbench_panel.h"

#include <QtCore/QTimer>

#include <QtWidgets/QAction>

#include <ini.h>
#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

#include <nx/client/desktop/event_search/widgets/event_panel.h>

using namespace nx::client::desktop::ui;

namespace NxUi {

NotificationsWorkbenchPanel::NotificationsWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    backgroundItem(new QnControlBackgroundWidget(Qt::RightEdge, parentWidget)),
    item(new QnNotificationsCollectionWidget(parentWidget, 0, context())),
    pinButton(NxUi::newPinButton(parentWidget, context(),
        action::PinNotificationsAction)),
    xAnimator(new VariantAnimator(this)),

    m_ignoreClickEvent(false),
    m_visible(false),
    m_showButton(NxUi::newBlinkingShowHideButton(parentWidget, context(),
        action::ToggleNotificationsAction)),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
{
    /* Notifications panel. */
    backgroundItem->setFrameBorders(Qt::LeftEdge);
    backgroundItem->setZValue(BackgroundItemZOrder);

    item->setZValue(ContentItemZOrder);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    setHelpTopic(item, Qn::MainWindow_Notifications_Help);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &NotificationsWorkbenchPanel::updateControlsGeometry);

    if (nx::client::desktop::ini().unifiedEventPanel)
    {
        item->setVisible(false);
        m_eventPanel.reset(new nx::client::desktop::EventPanel(context()));

        // TODO: #vkutin Get rid of proxying.
        auto eventPanelContainer = new QnMaskedProxyWidget(parentWidget);

        connect(item, &QGraphicsWidget::geometryChanged, this,
            [this, eventPanelContainer]()
            {
                eventPanelContainer->setGeometry(item->geometry());
            });

        // TODO: #vkutin Test which mode is faster.
        //eventPanelContainer->setCacheMode(QGraphicsItem::NoCache);
        eventPanelContainer->setWidget(m_eventPanel.data());
    }

    action(action::PinNotificationsAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    pinButton->setFocusProxy(item);
    pinButton->setZValue(ControlItemZOrder);
    connect(action(action::PinNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    action(action::ToggleNotificationsAction)->setChecked(settings.state == Qn::PaneState::Opened);
    m_showButton->setTransform(QTransform::fromScale(-1, 1));
    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(BackgroundItemZOrder); /*< To make it paint under the tooltip. */
    setHelpTopic(m_showButton, Qn::MainWindow_Pin_Help);
    item->setBlinker(m_showButton);
    connect(action(action::ToggleNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(m_showButton);
    m_showingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered);

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(pinButton));

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(item, item, Qt::LeftEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);

    updateControlsGeometry();
}

bool NotificationsWorkbenchPanel::isPinned() const
{
    return action(action::PinNotificationsAction)->isChecked();
}

bool NotificationsWorkbenchPanel::isOpened() const
{
    return action(action::ToggleNotificationsAction)->isChecked();
}

void NotificationsWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace nx::client::desktop::ui::workbench;

    ensureAnimationAllowed(&animate);

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(action::ToggleNotificationsAction)->setChecked(opened);

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::NotificationsPanelExpand
        : Animations::Id::NotificationsPanelCollapse);

    qreal width = item->size().width();
    qreal newX = m_parentWidget->rect().right()
        + (opened ? -width : kHidePanelOffset);

    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

    emit openedChanged(opened, animate);
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
        emit visibleChanged(visible, animate);
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
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        pinButton->setOpacity(opacity);
        backgroundItem->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }
}

bool NotificationsWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF NotificationsWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toReal());
    return geometry;
}

void NotificationsWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    item->setX(xAnimator->targetValue().toDouble());
}

void NotificationsWorkbenchPanel::setShowButtonUsed(bool used)
{
    m_showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void NotificationsWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();
    QRectF headerGeometry = m_parentWidget->mapRectFromItem(item, item->headerGeometry());
    QRectF backgroundGeometry = m_parentWidget->mapRectFromItem(item, item->visibleGeometry());

    QRectF paintGeometry = item->geometry();
    backgroundItem->setGeometry(paintGeometry);
    m_showButton->setPos(QPointF(
        qMin(parentWidgetRect.right(), paintGeometry.left()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height()) / 2
    ));

    if (nx::client::desktop::ini().unifiedEventPanel)
        pinButton->setPos(headerGeometry.topRight() + QPointF(1.0 - pinButton->size().width(), 1.0));
    else
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
