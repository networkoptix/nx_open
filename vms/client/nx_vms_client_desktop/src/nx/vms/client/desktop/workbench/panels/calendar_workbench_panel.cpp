// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_workbench_panel.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QLineEdit>
#include <QtGui/QIcon>

#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>

#include "../workbench_animations.h"

using namespace nx::vms::client::desktop;

namespace {

/* Make sure width is the same as notifications panel */
static const QSize kCalendarSize(250, 250);
static const QSize kDayTimeWidgetSize(250, 120);

static const int kClosedPositionOffsetY = 20;

/* Offset of pin button - in calendar header size count o_O */
static const int kPinOffsetCellsCount = 2;

QnFramedWidget* setupFrame(QGraphicsWidget* widget)
{
    const auto kFrameColor = toTransparent(colorTheme()->color("dark1"), 0.15);

    auto frame = new QnFramedWidget(widget);
    frame->setFlag(QGraphicsItem::ItemStacksBehindParent);
    frame->setWindowBrush(Qt::NoBrush);
    frame->setFrameColor(kFrameColor);
    frame->setPos(-frame->frameWidth(), -frame->frameWidth());

    const auto updateFrameSize =
        [widget, frame]()
        {
            const int sizeDelta = frame->frameWidth();
            frame->resize(widget->size() + QSize(sizeDelta, sizeDelta));
        };

    QObject::connect(widget, &QGraphicsWidget::geometryChanged, frame, updateFrameSize);
    updateFrameSize();

    return frame;
}

bool paintButtonFunction(QPainter* painter, const QStyleOption* /*option*/, const QWidget* widget)
{
    const QPushButton* thisButton = dynamic_cast<const QPushButton*>(widget);

    if (thisButton->isDown())
    {
        painter->fillRect(thisButton->rect(),
            QBrush(colorTheme()->color("calendar.pin_button.pressed")));
    }
    else if (thisButton->underMouse())
    {
        painter->fillRect(thisButton->rect(),
            QBrush(colorTheme()->color("calendar.pin_button.hovered")));
    }

    thisButton->icon().paint(painter, thisButton->rect(), Qt::AlignCenter,
        QIcon::Normal, thisButton->isChecked() ? QIcon::Off : QIcon::On);

    return true;
};

} // namespace

using namespace ui;

CalendarWorkbenchPanel::CalendarWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    base_type(settings, parentGraphicsWidget, parent),
    hidingProcessor(new HoverFocusProcessor(parentGraphicsWidget)),

    m_ignoreClickEvent(false),
    m_visible(false),
    m_origin(parentGraphicsWidget->geometry().bottomRight().toPoint()),
    m_widget(new QnCalendarWidget(parentWidget)),
    m_dayTimeWidget(new QnDayTimeWidget(parentWidget)),
    m_pinButton(new CustomPaintedButton(m_widget)),
    m_dayTimeMinimizeButton(new CustomPaintedButton(m_dayTimeWidget)),
    m_yAnimator(new VariantAnimator(this)),
    m_pinOffset(-kPinOffsetCellsCount * m_widget->headerHeight(), 0.0),
    m_dayTimeOpened(true),
    m_dayTimeOffset(-m_dayTimeWidget->headerHeight(), 0),
    m_opacityProcessor(new HoverFocusProcessor(parentGraphicsWidget))
{
    m_widget->setOpacity(kHidden);

    setHelpTopic(m_widget, Qn::MainWindow_Calendar_Help);

    navigator()->setCalendar(m_widget);
    connect(m_widget, &QnCalendarWidget::dateClicked, this,
        [this](const QDate& date)
        {
            const bool sameDate = (m_dayTimeWidget->date() == date);
            m_dayTimeWidget->setDate(date);

            if (isOpened())
            {
                const bool shouldBeOpened = !sameDate || !m_dayTimeOpened;
                setDayTimeWidgetOpened(shouldBeOpened);
            }
        });

    setHelpTopic(m_dayTimeWidget, Qn::MainWindow_DayTimePicker_Help);
    navigator()->setDayTimeWidget(m_dayTimeWidget);

    const int kShowHideAnimationPeriodMs = qnWorkbenchAnimations->timeLimit(
        nx::vms::client::desktop::ui::workbench::Animations::Id::CalendarShow);

    m_widget->resize(kCalendarSize);
    QSize dayTimeWidgetSize = m_dayTimeWidget->size();
    dayTimeWidgetSize.setWidth(m_widget->width());
    m_dayTimeWidget->resize(dayTimeWidgetSize);

    widgetOpacityAnimator(m_widget, display()->instrumentManager()->animationTimer())
        ->setTimeLimit(kShowHideAnimationPeriodMs);
    connect(m_widget, &QnCalendarWidget::geometryChanged,
        this, &CalendarWorkbenchPanel::updateControlsGeometry);

    m_pinButton->setCustomPaintFunction(paintButtonFunction);
    m_pinButton->setIcon(qnSkin->icon("panel/unpin_small.png", "panel/pin_small.png"));
    m_pinButton->setFixedSize(Skin::maximumSize(m_pinButton->icon()));
    m_pinButton->setCheckable(action(action::PinCalendarAction)->isCheckable());
    m_pinButton->setChecked(settings.state != Qn::PaneState::Unpinned);
    m_pinButton->setToolTip(action(action::PinCalendarAction)->toolTip());
    connect(m_pinButton, &QPushButton::clicked, this,
        [this]
        {
            action(action::PinCalendarAction)->setChecked(m_pinButton->isChecked());
        });

    action(action::PinCalendarAction)->setChecked(settings.state != Qn::PaneState::Unpinned);

    connect(action(action::PinCalendarAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    m_dayTimeMinimizeButton->setCustomPaintFunction(paintButtonFunction);
    m_dayTimeMinimizeButton->setIcon(qnSkin->icon("titlebar/dropdown.png"));
    m_dayTimeMinimizeButton->setFixedSize(Skin::maximumSize(m_dayTimeMinimizeButton->icon()));
    m_dayTimeMinimizeButton->setToolTip(action(action::MinimizeDayTimeViewAction)->toolTip());
    connect(m_dayTimeMinimizeButton, &QPushButton::clicked, this,
        [this]
        {
            action(action::MinimizeDayTimeViewAction)->trigger();
        });

    /* Hide pin/unpin button when any child line edit is visible: */
    installEventHandler(m_widget->findChildren<QLineEdit*>(), { QEvent::Show, QEvent::Hide }, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            m_pinButton->setVisible(event->type() == QEvent::Hide);
        });

    const auto toggleCalendarAction = action(action::ToggleCalendarAction);
    toggleCalendarAction->setChecked(settings.state == Qn::PaneState::Opened);
    connect(toggleCalendarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    connect(action(action::MinimizeDayTimeViewAction), &QAction::triggered, this,
        [this]
        {
            setDayTimeWidgetOpened(false);
        });

    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    hidingProcessor->setHoverLeaveDelay(kCloseCalendarPanelTimeoutMs);
    hidingProcessor->setFocusLeaveDelay(kCloseCalendarPanelTimeoutMs);
    connect(hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false, true);
        });

    for (auto widget: activeWidgets())
    {
        m_opacityProcessor->addTargetWidget(widget);
        hidingProcessor->addTargetWidget(widget);
    }

    m_yAnimator->setTimer(animationTimer());
    m_yAnimator->setTargetObject(m_widget);
    m_yAnimator->setAccessor(new PropertyAccessor("y"));
}

QList<QWidget*> CalendarWorkbenchPanel::activeWidgets() const
{
    return {m_widget, m_dayTimeWidget};
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
        // Make animation look a bit better.
        m_widget->setY(m_origin.y() + kClosedPositionOffsetY);
        setOpened(true, animated);
    }
    else
    {
        setVisible(false, animated);
        setDayTimeWidgetOpened(false);
    }
}

QPoint CalendarWorkbenchPanel::origin() const
{
    return m_origin;
}

void CalendarWorkbenchPanel::setOrigin(const QPoint& position)
{
    if (m_origin == position)
        return;

    bool animating = m_yAnimator->isRunning();
    m_yAnimator->stop();
    m_origin = position;
    QPoint targetPosition = position;
    if (!isOpened())
        targetPosition += QPoint(0, kClosedPositionOffsetY);

    if (animating)
        m_yAnimator->animateTo(targetPosition.y());
    else
        m_widget->move(targetPosition);
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
    using namespace nx::vms::client::desktop::ui::workbench;

    ensureAnimationAllowed(&animate);

    QScopedValueRollback<bool> guard(m_ignoreClickEvent, true);
    action(action::ToggleCalendarAction)->setChecked(opened);

    int newY = m_origin.y();
    if (!opened)
        newY += kClosedPositionOffsetY;

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::CalendarShow
        : Animations::Id::CalendarHide);

    if (animate)
        m_yAnimator->animateTo(newY);
    else
        m_widget->setY(newY);

    setVisible(opened && isEnabled(), animate);
    if (!opened)
        setDayTimeWidgetOpened(false);

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

    m_widget->setEnabled(visible);

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal CalendarWorkbenchPanel::opacity() const
{
    return m_widget->opacity();
}

void CalendarWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        widgetOpacityAnimator(m_widget, display()->instrumentManager()->animationTimer())
            ->animateTo(opacity * masterOpacity());
    }
    else
    {
        m_widget->setOpacity(opacity * masterOpacity());
    }
}

bool CalendarWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF CalendarWorkbenchPanel::geometry() const
{
    return m_widget->geometry();
}

void CalendarWorkbenchPanel::setPanelSize(qreal /*size*/)
{
    // Do nothing here.
    // This panel is automatically adjusted to size of notifications panel.
}

QRectF CalendarWorkbenchPanel::effectiveGeometry() const
{
    QRect geometry = m_widget->geometry();
    if (m_yAnimator->isRunning())
        geometry.moveTop(m_yAnimator->targetValue().toInt());
    if (m_dayTimeOpened)
        geometry.setTop(geometry.top() - m_dayTimeWidget->geometry().height());
    return geometry;
}

void CalendarWorkbenchPanel::stopAnimations()
{
    if (!m_yAnimator->isRunning())
        return;

    m_yAnimator->stop();
    m_widget->setY(m_yAnimator->targetValue().toInt());
}

void CalendarWorkbenchPanel::setDayTimeWidgetOpened(bool opened)
{
    if (m_dayTimeOpened == opened)
        return;

    m_dayTimeOpened = opened;
    m_dayTimeWidget->setVisible(opened);

    emit geometryChanged();
}

void CalendarWorkbenchPanel::updateControlsGeometry()
{
    const QRect geometry = m_widget->geometry();
    m_pinButton->move(QPoint(geometry.width() + m_pinOffset.x(), m_pinOffset.y()));
    const QPoint dayTimeOffset(kDayTimeWidgetSize.width(), kDayTimeWidgetSize.height());
    m_dayTimeWidget->move(geometry.topRight() - dayTimeOffset);
    const QSize dateTimeSize(geometry.width(), geometry.y() - m_dayTimeWidget->y());
    m_dayTimeWidget->resize(dateTimeSize);
    m_dayTimeMinimizeButton->move(QPoint(m_dayTimeWidget->width() + m_dayTimeOffset.x(), m_dayTimeOffset.y()));
    emit geometryChanged();
}
