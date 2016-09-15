#include "calendar_workbench_panel.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

namespace {

/* Make sure width is the same as notifications panel */
static const QSize kCalendarSize(250, 250);
static const QSize kDayTimeWidgetSize(250, 120);

static const qreal kClosedPositionOffsetY = 20;

}

namespace NxUi {

CalendarWorkbenchPanel::CalendarWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    inGeometryUpdate(false),
    inDayTimeGeometryUpdate(false),
    dayTimeOpened(false),

    m_ignoreClickEvent(false),
    m_visible(false)
{
    widget = new QnCalendarWidget();
    setHelpTopic(widget, Qn::MainWindow_Calendar_Help);
    navigator()->setCalendar(widget);
    connect(widget, &QnCalendarWidget::dateClicked, this,
        &CalendarWorkbenchPanel::at_widget_dateClicked);

    dayTimeWidget = new QnDayTimeWidget();
    setHelpTopic(dayTimeWidget, Qn::MainWindow_DayTimePicker_Help);
    navigator()->setDayTimeWidget(dayTimeWidget);

    static const int kCellsCountOffset = 2;
    const int size = widget->headerHeight();
    pinOffset = QPoint(-kCellsCountOffset * size, 0.0);
    dayTimeOffset = QPoint(-dayTimeWidget->headerHeight(), 0);

    item = new QnMaskedProxyWidget(parentWidget);
    item->setWidget(widget);
    widget->installEventFilter(item);
    item->resize(kCalendarSize);
    opacityAnimator(item)->setTimeLimit(50);

    connect(item, &QGraphicsWidget::geometryChanged, this,
        &CalendarWorkbenchPanel::updateControlsGeometry);

    item->setProperty(Qn::NoHandScrollOver, true);

    const auto pinCalendarAction = action(QnActions::PinCalendarAction);
    pinCalendarAction->setChecked(settings.state != Qn::PaneState::Unpinned);
    pinButton = newPinButton(parentWidget, context(), pinCalendarAction, true);
    pinButton->setFocusProxy(item);

    const auto toggleCalendarAction = action(QnActions::ToggleCalendarAction);
    toggleCalendarAction->setChecked(settings.state == Qn::PaneState::Opened);
    connect(toggleCalendarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    dayTimeItem = new QnMaskedProxyWidget(parentWidget);
    dayTimeItem->setWidget(dayTimeWidget);
    dayTimeWidget->installEventFilter(item);
    dayTimeItem->resize(kDayTimeWidgetSize);
    dayTimeItem->setProperty(Qn::NoHandScrollOver, true);
    dayTimeItem->stackBefore(item);

    connect(dayTimeItem, &QnMaskedProxyWidget::paintRectChanged, this,
        &CalendarWorkbenchPanel::at_dayTimeItem_paintGeometryChanged);
    connect(dayTimeItem, &QGraphicsWidget::geometryChanged, this,
        &CalendarWorkbenchPanel::at_dayTimeItem_paintGeometryChanged);

    dayTimeMinimizeButton = newActionButton(parentWidget, context(),
        action(QnActions::MinimizeDayTimeViewAction), Qn::Empty_Help);
    dayTimeMinimizeButton->setFocusProxy(dayTimeItem);
    connect(action(QnActions::MinimizeDayTimeViewAction), &QAction::triggered, this,
        [this]
        {
            setDayTimeWidgetOpened(false, true);
        });

    opacityProcessor = new HoverFocusProcessor(parentWidget);
    opacityProcessor->addTargetItem(item);
    opacityProcessor->addTargetItem(dayTimeItem);
    opacityProcessor->addTargetItem(pinButton);
    opacityProcessor->addTargetItem(dayTimeMinimizeButton);

    hidingProcessor = new HoverFocusProcessor(parentWidget);
    hidingProcessor->addTargetItem(item);
    hidingProcessor->addTargetItem(dayTimeItem);
    hidingProcessor->addTargetItem(pinButton);
    hidingProcessor->setHoverLeaveDelay(kClosePanelTimeoutMs);
    hidingProcessor->setFocusLeaveDelay(kClosePanelTimeoutMs);
    connect(hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    yAnimator = new VariantAnimator(this);
    yAnimator->setTimer(animationTimer());
    yAnimator->setTargetObject(item);
    yAnimator->setAccessor(new PropertyAccessor("y"));
    yAnimator->setTimeLimit(50);

    dayTimeSizeAnimator = new VariantAnimator(this);
    dayTimeSizeAnimator->setTimer(animationTimer());
    dayTimeSizeAnimator->setTargetObject(dayTimeItem);
    dayTimeSizeAnimator->setAccessor(new PropertyAccessor("paintSize"));
    dayTimeSizeAnimator->setTimeLimit(50);

    opacityAnimatorGroup = new AnimatorGroup(this);
    opacityAnimatorGroup->setTimer(animationTimer());
    opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    opacityAnimatorGroup->addAnimator(opacityAnimator(dayTimeItem));
    opacityAnimatorGroup->addAnimator(opacityAnimator(pinButton));
    opacityAnimatorGroup->addAnimator(opacityAnimator(dayTimeMinimizeButton));
    opacityAnimatorGroup->setTimeLimit(50);
}

bool CalendarWorkbenchPanel::isEnabled() const
{
    return action(QnActions::ToggleCalendarAction)->isEnabled();
}

void CalendarWorkbenchPanel::setEnabled(bool enabled)
{
    if (isEnabled() == enabled)
        return;

    action(QnActions::ToggleCalendarAction)->setEnabled(enabled);
    if (!enabled)
        setOpened(false, false);
}

QPointF CalendarWorkbenchPanel::origin() const
{
    return m_origin;
}

void CalendarWorkbenchPanel::setOrigin(const QPointF& position)
{
    if (qFuzzyEquals(m_origin, position))
        return;
    m_origin = position;
    QPointF targetPosition = position;
    if (!isOpened())
        targetPosition += QPointF(0, kClosedPositionOffsetY);
    item->setPos(targetPosition);
}

bool CalendarWorkbenchPanel::isPinned() const
{
    return action(QnActions::PinCalendarAction)->isChecked();
}

bool CalendarWorkbenchPanel::isOpened() const
{
    return action(QnActions::ToggleCalendarAction)->isChecked();
}

void CalendarWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);
    if (!action(QnActions::ToggleCalendarAction)->isEnabled())
        opened = false;

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleCalendarAction)->setChecked(opened);

    qreal newY = m_origin.y();
    if (!opened)
        newY += kClosedPositionOffsetY;

    setVisible(opened, animate);

    yAnimator->stop();
    if (opened)
        yAnimator->setEasingCurve(QEasingCurve::OutCubic);
    else
        yAnimator->setEasingCurve(QEasingCurve::InCubic);

    if (animate)
        yAnimator->animateTo(newY);
    else
        item->setY(newY);

    if (!opened)
        setDayTimeWidgetOpened(opened, animate);

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
    return opacityAnimator(item)->targetValue().toDouble();
}

void CalendarWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(dayTimeItem)->setTargetValue(opacity);
        opacityAnimator(pinButton)->setTargetValue(opacity);
        opacityAnimator(dayTimeMinimizeButton)->setTargetValue(opacity);
        opacityAnimatorGroup->start();
    }
    else
    {
        opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        dayTimeItem->setOpacity(opacity);
        pinButton->setOpacity(opacity);
        dayTimeMinimizeButton->setOpacity(opacity);
    }
}

bool CalendarWorkbenchPanel::isHovered() const
{
    return opacityProcessor->isHovered();
}


QRectF CalendarWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (yAnimator->isRunning())
        geometry.moveTop(yAnimator->targetValue().toReal());
    if (dayTimeOpened)
        geometry.setTop(geometry.top() - dayTimeItem->geometry().height());
    return geometry;
}

void CalendarWorkbenchPanel::setDayTimeWidgetOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    dayTimeOpened = opened;

    QSizeF newSize(dayTimeItem->size());
    if (!opened)
        newSize.setHeight(0.0);

    if (animate)
    {
        dayTimeSizeAnimator->animateTo(newSize);
    }
    else
    {
        dayTimeSizeAnimator->stop();
        dayTimeItem->setPaintSize(newSize);
    }
}

void CalendarWorkbenchPanel::setProxyUpdatesEnabled(bool updatesEnabled)
{
    base_type::setProxyUpdatesEnabled(updatesEnabled);
    item->setUpdatesEnabled(updatesEnabled);
}

void CalendarWorkbenchPanel::updateControlsGeometry()
{
    const QRectF geometry = item->geometry();
    pinButton->setPos(geometry.topRight() + pinOffset);
    updateDayTimeWidgetGeometry();

    emit geometryChanged();
}

void CalendarWorkbenchPanel::updateDayTimeWidgetGeometry()
{
    /* Update painting rect the "fair" way. */
    const QRectF calendarGeometry = item->geometry();
    QRectF geometry = dayTimeItem->geometry();
    geometry.moveRight(calendarGeometry.right());
    geometry.moveBottom(calendarGeometry.top());
    dayTimeItem->setGeometry(geometry);
}

void CalendarWorkbenchPanel::at_widget_dateClicked(const QDate &date)
{
    const bool sameDate = (dayTimeWidget->date() == date);
    dayTimeWidget->setDate(date);

    if (isOpened())
    {
        const bool shouldBeOpened = !sameDate || !dayTimeOpened;
        setDayTimeWidgetOpened(shouldBeOpened, true);
    }
}

void CalendarWorkbenchPanel::at_dayTimeItem_paintGeometryChanged()
{
    const QRectF paintGeomerty = dayTimeItem->paintGeometry();
    dayTimeMinimizeButton->setPos(paintGeomerty.topRight() + dayTimeOffset);
    dayTimeMinimizeButton->setVisible(paintGeomerty.height());

    if (inDayTimeGeometryUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&inDayTimeGeometryUpdate, true);

    emit geometryChanged();
}

} //namespace NxUi
