// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_workbench_panel.h"

#include <QtGui/QAction>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/scene/widgets/timeline_calendar_widget.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_pane_settings.h>

namespace nx::vms::client::desktop {

using namespace ui;

CalendarWorkbenchPanel::CalendarWorkbenchPanel(
    const QnPaneSettings& settings,
    QWidget* parentWidget,
    QGraphicsWidget* parentGraphicsWidget,
    QObject* parent)
    :
    AbstractWorkbenchPanel(settings, parentGraphicsWidget, parent),
    m_widget(new TimelineCalendarWidget(parentWidget))
{
    m_widget->setVisible(false);

    setHelpTopic(m_widget, HelpTopic::Id::MainWindow_Calendar);

    navigator()->setCalendar(m_widget);

    const auto toggleCalendarAction = action(action::ToggleCalendarAction);
    toggleCalendarAction->setChecked(settings.state == Qn::PaneState::Opened);
    connect(toggleCalendarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            setOpened(checked, true);
        });
}

TimelineCalendarWidget* CalendarWorkbenchPanel::widget() const
{
    return m_widget;
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
        setOpened(true, animated);
    }
    else
    {
        setVisible(false, animated);
    }
}

QPoint CalendarWorkbenchPanel::position() const
{
    return m_widget->pos();
}

void CalendarWorkbenchPanel::setPosition(const QPoint& position)
{
    m_widget->move(position);
}

bool CalendarWorkbenchPanel::isPinned() const
{
    return true;
}

bool CalendarWorkbenchPanel::isOpened() const
{
    return action(action::ToggleCalendarAction)->isChecked();
}

void CalendarWorkbenchPanel::setOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(&animate);

    action(action::ToggleCalendarAction)->setChecked(opened);

    setVisible(opened && isEnabled(), animate);

    emit openedChanged(opened, animate);
}

bool CalendarWorkbenchPanel::isVisible() const
{
    return m_widget->isVisible();
}

void CalendarWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (m_widget->isVisible() == visible)
        return;

    m_widget->setVisible(visible);
    m_widget->setEnabled(visible);
    updateOpacity(animate);

    emit visibleChanged(visible, animate);
}

qreal CalendarWorkbenchPanel::opacity() const
{
    return m_widget->opacity;
}

void CalendarWorkbenchPanel::setOpacity(qreal opacity, bool /*animate*/)
{
    m_widget->opacity = opacity * masterOpacity();
}

bool CalendarWorkbenchPanel::isHovered() const
{
    return m_widget->hovered;
}

QRectF CalendarWorkbenchPanel::geometry() const
{
    return m_widget->geometry();
}

void CalendarWorkbenchPanel::setPanelSize(qreal /*size*/)
{
    // Do nothing here. This panel adjusts to its contents.
}

QRectF CalendarWorkbenchPanel::effectiveGeometry() const
{
    return m_widget->geometry();
}

void CalendarWorkbenchPanel::stopAnimations()
{
    // There are no animations in this panel.
}

} // namespace nx::vms::client::desktop
