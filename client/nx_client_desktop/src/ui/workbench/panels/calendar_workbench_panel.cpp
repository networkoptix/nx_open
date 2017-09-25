#include "calendar_workbench_panel.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QLineEdit>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/panels/buttons.h>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui;

namespace {

/* Make sure width is the same as notifications panel */
static const QSize kCalendarSize(250, 250);
static const QSize kDayTimeWidgetSize(250, 120);

static const qreal kClosedPositionOffsetY = 20;

/* Offset of pin button - in calendar header size count o_O */
static const int kPinOffsetCellsCount = 2;

}

namespace NxUi {

CalendarWorkbenchPanel::CalendarWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    item(new QnMaskedProxyWidget(parentWidget)),
    hidingProcessor(new HoverFocusProcessor(parentWidget)),

    m_ignoreClickEvent(false),
    m_visible(false),
    m_origin(parentWidget->geometry().bottomRight()),
    m_widget(new QnCalendarWidget()),
    m_pinButton(newPinButton(parentWidget, context(),
        action::PinCalendarAction, true)),
    m_dayTimeMinimizeButton(newActionButton(parentWidget, context(),
        action::MinimizeDayTimeViewAction, Qn::Empty_Help)),
    m_yAnimator(new VariantAnimator(this)),
    m_pinOffset(-kPinOffsetCellsCount * m_widget->headerHeight(), 0.0),
    m_dayTimeOpened(true),
    m_dayTimeItem(new QnMaskedProxyWidget(parentWidget)),
    m_dayTimeWidget(new QnDayTimeWidget()),
    m_dayTimeOffset(-m_dayTimeWidget->headerHeight(), 0),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
{
    setHelpTopic(m_widget, Qn::MainWindow_Calendar_Help);
    navigator()->setCalendar(m_widget);
    connect(m_widget, &QnCalendarWidget::dateClicked, this,
        &CalendarWorkbenchPanel::at_widget_dateClicked);

    setHelpTopic(m_dayTimeWidget, Qn::MainWindow_DayTimePicker_Help);
    navigator()->setDayTimeWidget(m_dayTimeWidget);

    const int kShowHideAnimationPeriodMs = qnWorkbenchAnimations->timeLimit(
        nx::client::desktop::ui::workbench::Animations::Id::CalendarShow);

    item->setWidget(m_widget);
    item->resize(kCalendarSize);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    item->setZValue(ContentItemZOrder);
    opacityAnimator(item)->setTimeLimit(kShowHideAnimationPeriodMs);
    m_widget->installEventFilter(item);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &CalendarWorkbenchPanel::updateControlsGeometry);

    /* Hide pin/unpin button when any child line edit is visible: */
    installEventHandler(m_widget->findChildren<QLineEdit*>(), { QEvent::Show, QEvent::Hide }, this,
        [this](QObject* object, QEvent* event)
        {
            Q_UNUSED(object);
            m_pinButton->setVisible(event->type() == QEvent::Hide);
        });

    action(action::PinCalendarAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    m_pinButton->setFocusProxy(item);
    m_pinButton->setZValue(ControlItemZOrder);

    connect(action(action::PinCalendarAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    const auto toggleCalendarAction = action(action::ToggleCalendarAction);
    toggleCalendarAction->setChecked(settings.state == Qn::PaneState::Opened);
    connect(toggleCalendarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_dayTimeItem->setWidget(m_dayTimeWidget);
    m_dayTimeWidget->installEventFilter(item);
    m_dayTimeItem->resize(kDayTimeWidgetSize);
    m_dayTimeItem->setProperty(Qn::NoHandScrollOver, true);
    m_dayTimeItem->setProperty(Qn::BlockMotionSelection, true);
    m_dayTimeItem->setZValue(ContentItemZOrder);
    opacityAnimator(m_dayTimeItem)->setTimeLimit(kShowHideAnimationPeriodMs);

    m_dayTimeMinimizeButton->setFocusProxy(m_dayTimeItem);
    m_dayTimeMinimizeButton->setZValue(ControlItemZOrder);
    opacityAnimator(m_dayTimeMinimizeButton)->setTimeLimit(kShowHideAnimationPeriodMs);
    connect(action(action::MinimizeDayTimeViewAction), &QAction::triggered, this,
        [this]
        {
            setDayTimeWidgetOpened(false, true);
        });

    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    hidingProcessor->setHoverLeaveDelay(kClosePanelTimeoutMs);
    hidingProcessor->setFocusLeaveDelay(kClosePanelTimeoutMs);
    connect(hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    for (auto item : activeItems())
    {
        m_opacityProcessor->addTargetItem(item);
        hidingProcessor->addTargetItem(item);
    }

    m_yAnimator->setTimer(animationTimer());
    m_yAnimator->setTargetObject(item);
    m_yAnimator->setAccessor(new PropertyAccessor("y"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_pinButton));
    m_opacityAnimatorGroup->setTimeLimit(kShowHideAnimationPeriodMs);
}

QList<QGraphicsItem*> CalendarWorkbenchPanel::activeItems() const
{
    return {
        item,
        m_dayTimeItem,
        m_pinButton,
        m_dayTimeMinimizeButton };
}

bool CalendarWorkbenchPanel::isEnabled() const
{
    return action(action::ToggleCalendarAction)->isEnabled();
}

void CalendarWorkbenchPanel::setEnabled(bool enabled, bool animated)
{
    if (isEnabled() == enabled)
        return;

    action(action::ToggleCalendarAction)->setEnabled(enabled);
    if (!isOpened())
        return;

    if (enabled)
    {
        /* Minor hack to make animation look better. */
        item->setY(m_origin.y() + kClosedPositionOffsetY);
        setOpened(true, animated);
    }
    else
    {
        setVisible(false, animated);
        setDayTimeWidgetOpened(false, animated);
    }
}

QPointF CalendarWorkbenchPanel::origin() const
{
    return m_origin;
}

void CalendarWorkbenchPanel::setOrigin(const QPointF& position)
{
    if (qFuzzyEquals(m_origin, position))
        return;

    bool animating = m_yAnimator->isRunning();
    m_yAnimator->stop();
    m_origin = position;
    QPointF targetPosition = position;
    if (!isOpened())
        targetPosition += QPointF(0, kClosedPositionOffsetY);

    if (animating)
        m_yAnimator->animateTo(targetPosition.y());
    else
        item->setPos(targetPosition);
}

bool CalendarWorkbenchPanel::isPinned() const
{
    return action(action::PinCalendarAction)->isChecked();
}

bool CalendarWorkbenchPanel::isOpened() const
{
    return action(action::ToggleCalendarAction)->isChecked();
}

void CalendarWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace nx::client::desktop::ui::workbench;

    ensureAnimationAllowed(&animate);

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(action::ToggleCalendarAction)->setChecked(opened);

    qreal newY = m_origin.y();
    if (!opened)
        newY += kClosedPositionOffsetY;

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::CalendarShow
        : Animations::Id::CalendarHide);

    if (animate)
        m_yAnimator->animateTo(newY);
    else
        item->setY(newY);

    setVisible(opened && isEnabled(), animate);
    if (!opened)
        setDayTimeWidgetOpened(false, animate);

    emit openedChanged(opened, animate);
}

bool CalendarWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void CalendarWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal CalendarWorkbenchPanel::opacity() const
{
    return opacityAnimator(m_pinButton)->targetValue().toDouble();
}

void CalendarWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity * masterOpacity());
        opacityAnimator(m_pinButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity * masterOpacity());
        m_pinButton->setOpacity(opacity);
    }
}

bool CalendarWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF CalendarWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (m_yAnimator->isRunning())
        geometry.moveTop(m_yAnimator->targetValue().toReal());
    if (m_dayTimeOpened)
        geometry.setTop(geometry.top() - m_dayTimeItem->geometry().height());
    return geometry;
}

void CalendarWorkbenchPanel::stopAnimations()
{
    if (!m_yAnimator->isRunning())
        return;

    m_yAnimator->stop();
    item->setY(m_yAnimator->targetValue().toDouble());
}

void CalendarWorkbenchPanel::setDayTimeWidgetOpened(bool opened, bool animate)
{
    if (m_dayTimeOpened == opened)
        return;
    m_dayTimeOpened = opened;

    ensureAnimationAllowed(&animate);
    qreal opacity = opened ? kOpaque : kHidden;
    if (animate)
    {
        opacityAnimator(m_dayTimeItem)->animateTo(opacity * masterOpacity());
        opacityAnimator(m_dayTimeMinimizeButton)->animateTo(opacity);
    }
    else
    {
        m_dayTimeItem->setOpacity(opacity * masterOpacity());
        m_dayTimeMinimizeButton->setOpacity(opacity);
    }

    emit geometryChanged();
}

void CalendarWorkbenchPanel::setProxyUpdatesEnabled(bool updatesEnabled)
{
    base_type::setProxyUpdatesEnabled(updatesEnabled);
    item->setUpdatesEnabled(updatesEnabled);
}

void CalendarWorkbenchPanel::updateControlsGeometry()
{
    const QRectF geometry = item->geometry();
    m_pinButton->setPos(geometry.topRight() + m_pinOffset);
    const QPoint dayTimOffset(kDayTimeWidgetSize.width(), kDayTimeWidgetSize.height());
    m_dayTimeItem->setPos(geometry.topRight() - dayTimOffset);
    m_dayTimeMinimizeButton->setPos(m_dayTimeItem->geometry().topRight() + m_dayTimeOffset);
    emit geometryChanged();
}

void CalendarWorkbenchPanel::at_widget_dateClicked(const QDate &date)
{
    const bool sameDate = (m_dayTimeWidget->date() == date);
    m_dayTimeWidget->setDate(date);

    if (isOpened())
    {
        const bool shouldBeOpened = !sameDate || !m_dayTimeOpened;
        setDayTimeWidgetOpened(shouldBeOpened, true);
    }
}

} //namespace NxUi
