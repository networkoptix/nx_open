// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notifications_workbench_panel.h"

#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/event_search/widgets/advanced_search_dialog.h>
#include <nx/vms/client/desktop/event_search/widgets/event_panel.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/event_processors.h>

#include "buttons.h"

namespace nx::vms::client::desktop {

using namespace ui;

namespace {

int narrowWidth() { return ini().newPanelsLayout ? 264 : 283; }

static constexpr int kWideWidth = 433;

} // namespace

BaseWidget::BaseWidget(QWidget* parent):
    QWidget(parent)
{
    installEventHandler({this}, {QEvent::Resize, QEvent::Move},
        this, &BaseWidget::geometryChanged);
}

BaseWidget::~BaseWidget()
{
}

int BaseWidget::x() const
{
    return pos().x();
}

void BaseWidget::setX(int x)
{
    QPoint pos_ = pos();

    if (pos_.x() != x)
    {
        move(x, pos_.y());
        emit xChanged(x);
    }
}

class NotificationsWorkbenchPanel::ResizerWidget:
    public QGraphicsWidget,
    public DragProcessHandler
{
public:
    ResizerWidget(QWidget* toResize, QGraphicsItem* parent):
        QGraphicsWidget(parent),
        DragProcessHandler(),
        m_itemToResize(toResize)
    {
        NX_ASSERT(m_itemToResize);

        setCursor(Qt::SizeHorCursor);
        setFlag(QGraphicsItem::ItemHasNoContents);
    }

    void setWidth(int newWidth)
    {
        static const auto kThreshold = (narrowWidth() + kWideWidth) / 2;
        static constexpr int kBoundaryWidth = 8;

        int newMinimumWidth = m_itemToResize->minimumWidth();

        if (newWidth > kThreshold + kBoundaryWidth)
            newMinimumWidth = kWideWidth;
        else if (newWidth < kThreshold - kBoundaryWidth)
            newMinimumWidth = narrowWidth();

        const int sizeDelta = m_itemToResize->minimumWidth() - newMinimumWidth;
        if (sizeDelta == 0)
            return;

        const QRect newGeometry(
            m_itemToResize->pos().x() + sizeDelta,
            m_itemToResize->pos().y(),
            newMinimumWidth,
            m_itemToResize->size().height());

        m_itemToResize->setMinimumWidth(newMinimumWidth);
        m_itemToResize->setGeometry(newGeometry);
    }

protected:
    virtual void dragMove(DragInfo* info) override
    {
        if (!m_itemToResize)
            return;

        const auto delta = info->mouseItemPos().x() - info->mousePressItemPos().x();
        const auto newWidth = m_itemToResize->minimumWidth() - delta;

        setWidth(newWidth);
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mousePressEvent(this, event);
        event->accept();
    }

    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mouseMoveEvent(this, event);
        event->accept();
    }

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mouseReleaseEvent(this, event);
        event->accept();
    }

private:
    QWidget* const m_itemToResize = nullptr;
};

NotificationsWorkbenchPanel::NotificationsWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    backgroundItem(new QnControlBackgroundWidget(parentGraphicsWidget)),
    xAnimator(new VariantAnimator(this)),
    m_showButton(newBlinkingShowHideButton(parentGraphicsWidget, windowContext(),
        menu::ToggleNotificationsAction)),
    m_opacityAnimatorGroup(new AnimatorGroup(this)),
    m_baseWidget(new BaseWidget(parentWidget)),
    m_eventPanelHoverProcessor(new HoverFocusProcessor(backgroundItem))
{
    /* Notifications panel. */
    backgroundItem->setFrameBorders(Qt::LeftEdge);
    backgroundItem->setZValue(BackgroundItemZOrder);

    initEventPanel();

    setHelpTopic(m_widget, HelpTopic::Id::NotificationsPanel);

    action(menu::ToggleNotificationsAction)->setChecked(settings.state == Qn::PaneState::Opened);
    m_showButton->setTransform(QTransform::fromScale(-1, 1)); // Achtung! Flips button horizontally.
    connect(action(menu::ToggleNotificationsAction), &QAction::toggled,
        this, [this](bool opened) { if (!m_blockAction) setOpened(opened); });

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(m_baseWidget);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));

    updateControlsGeometry();

    AdvancedSearchDialog::registerStateDelegate();
}

NotificationsWorkbenchPanel::~NotificationsWorkbenchPanel()
{
    AdvancedSearchDialog::unregisterStateDelegate();
}

void NotificationsWorkbenchPanel::setX(qreal x)
{
    m_baseWidget->setX(x);
}

void NotificationsWorkbenchPanel::setPos(const QPoint& pos)
{
    QRect geometry = m_baseWidget->geometry();
    geometry.setTopLeft(pos);
    m_baseWidget->setGeometry(geometry);
}

void NotificationsWorkbenchPanel::resize(const QSize& size)
{
    QRect geometry = m_baseWidget->geometry();
    geometry.setSize(size);
    m_baseWidget->setGeometry(geometry);
}

QRectF NotificationsWorkbenchPanel::geometry() const
{
    return m_baseWidget->geometry();
}

bool NotificationsWorkbenchPanel::isPinned() const
{
    return true;
}

bool NotificationsWorkbenchPanel::isOpened() const
{
    return action(menu::ToggleNotificationsAction)->isChecked();
}

void NotificationsWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;

    ensureAnimationAllowed(&animate);

    auto toggleAction = action(menu::ToggleNotificationsAction);
    m_blockAction = true;
    toggleAction->setChecked(opened);
    m_blockAction = false;

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::NotificationsPanelExpand
        : Animations::Id::NotificationsPanelCollapse);

    qreal width = m_baseWidget->size().width();
    int newX = m_parentWidget->rect().right() + (opened ? -width : kHidePanelOffset);

    m_widget->setVisible(true);

    if (animate)
        xAnimator->animateTo(newX);
    else
        m_baseWidget->move(static_cast<int>(newX), m_baseWidget->y());

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
    m_baseWidget->setVisible(visible);

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal NotificationsWorkbenchPanel::opacity() const
{
    return opacityAnimator(backgroundItem)->targetValue().toDouble();
}

void NotificationsWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(backgroundItem)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        backgroundItem->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }
}

bool NotificationsWorkbenchPanel::isHovered() const
{
    return false;
}

void NotificationsWorkbenchPanel::setPanelSize(qreal size)
{
    if (resizeable())
        m_panelResizer->setWidth(size);
}

QRectF NotificationsWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = m_baseWidget->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toReal());
    return geometry;
}

void NotificationsWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    m_baseWidget->setX(xAnimator->targetValue().toInt());
}

void NotificationsWorkbenchPanel::enableShowButton(bool enabled)
{
    m_showButton->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
}

void NotificationsWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();

    QRectF paintGeometry = m_baseWidget->geometry();
    backgroundItem->setGeometry(paintGeometry);
    m_showButton->setPos(QPointF(
        qMin(parentWidgetRect.right(), paintGeometry.left()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height()) / 2));

    emit geometryChanged();
}

void NotificationsWorkbenchPanel::initEventPanel()
{
    m_baseWidget->setLayout(new QVBoxLayout());
    m_widget = new EventPanel(windowContext(), m_baseWidget);
    m_baseWidget->layout()->addWidget(m_widget);
    m_baseWidget->setMinimumSize(m_widget->size());

    m_panelResizer.reset(new ResizerWidget(m_baseWidget, backgroundItem));
    m_panelResizer->setWidth(narrowWidth());
    m_panelResizer->setVisible(m_resizeable);
    auto dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(m_panelResizer.data());

    connect(m_baseWidget, &BaseWidget::geometryChanged, this,
        [this]
        {
            // Add 1-pixel shift for notification panel frame.
            const auto newGeometry = m_baseWidget->geometry().adjusted(1, 0, 0, 0);
            backgroundItem->setGeometry(newGeometry);
            m_panelResizer->setGeometry(-style::Metrics::kStandardPadding, 0, style::Metrics::kStandardPadding,
                backgroundItem->size().height());

            updateControlsGeometry();
            geometryChanged();
        });

    connect(m_baseWidget, &BaseWidget::geometryChanged, this,
        [this]
        {
            // Hide EventPanel widget when the panel is moved out of view.
            // This is required for correct counting of unread notifications.
            const bool shouldBeHidden = m_baseWidget->x() >= (int)m_parentWidget->rect().right();

            // Avoid unneeded flickering.
            if (m_widget->isHidden() == shouldBeHidden)
                return;

            // Visibility changes should not affect scene focus. Qt implementation automatically
            // focuses newly appeared graphics item.
            const auto currentFocusItem = m_parentWidget->scene()->focusItem();
            m_widget->setHidden(shouldBeHidden);
            if (shouldBeHidden)
                m_widget->clearFocus();
            else
                m_parentWidget->scene()->setFocusItem(currentFocusItem);
        });

    connect(m_widget, &EventPanel::unreadCountChanged, this,
        [this](int count, QnNotificationLevel::Value level)
        {
            m_showButton->setNotificationCount(count);
            m_showButton->setColor(QnNotificationLevel::notificationColor(level));
        });

    connect(m_widget, &EventPanel::currentTabChanged, this,
        [this](EventPanel::Tab tab)
        {
            if (tab == EventPanel::Tab::motion && !display()->widget(Qn::ZoomedRole))
                setOpened();
        });
}

bool NotificationsWorkbenchPanel::resizeable() const
{
    return m_resizeable;
}

void NotificationsWorkbenchPanel::setResizeable(bool value)
{
    m_resizeable = value;
    m_panelResizer->setVisible(value);
}

} //namespace nx::vms::client::desktop
