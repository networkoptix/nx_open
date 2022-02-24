// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_panel.h"

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsSceneResizeEvent>
#include <QtWidgets/QSizePolicy>

#include <ui/graphics/items/controls/time_slider.h>
#include <ui/help/help_topics.h>
#include <utils/math/functors.h>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

using std::chrono::milliseconds;

namespace nx::vms::client::desktop::workbench::timeline {

ThumbnailPanel::ThumbnailPanel(QnTimeSlider* slider, QGraphicsItem *parent):
    base_type(parent),
    m_slider(slider)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptHoverEvents(true);
}

ThumbnailPanel::~ThumbnailPanel()
{
}

void ThumbnailPanel::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    if (rect().isEmpty())
        return;

    base_type::paint(painter, option, widget);
    painter->fillRect(rect(), colorTheme()->color("dark5"));
    painter->setPen(colorTheme()->color("dark6"));
    painter->drawLine(0, 0, rect().width(), 0);

    const QnTimePeriod fullPeriod = fullTimePeriod();
    const QnLinearFunction unboundMapper(
        fullPeriod.startTimeMs, positionFromTime(fullPeriod.startTime()),
        fullPeriod.endTimeMs(), positionFromTime(fullPeriod.endTime()));
    const QnBoundedLinearFunction boundMapper(unboundMapper, rect().left(), rect().right());

    m_slider->drawThumbnails(painter, rect(), fullPeriod, unboundMapper, boundMapper);
}

int ThumbnailPanel::helpTopicAt(const QPointF&) const
{
    return Qn::MainWindow_Thumbnails_Help;
}

QnTimePeriod ThumbnailPanel::fullTimePeriod() const
{
    return QnTimePeriod::fromInterval(
        timeFromPosition({rect().left(), 0}), timeFromPosition({rect().right(), 0}));
}

void ThumbnailPanel::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    m_slider->handleThumbnailsWheel(event);
}

void ThumbnailPanel::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    base_type::resizeEvent(event);

    if (event->oldSize() == event->newSize())
        return;

    m_slider->handleThumbnailsResize();
}

void ThumbnailPanel::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverMoveEvent(event);

    m_slider->handleThumbnailsHoverMove(timeFromPosition(event->pos()));
}

void ThumbnailPanel::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_slider->handleThumbnailsMousePress(
        timeFromPosition(event->pos()), event->button());
    event->accept();
}

qreal ThumbnailPanel::positionFromTime(milliseconds time) const
{
    return mapFromItem(m_slider, m_slider->positionFromTime(time, /*bound*/ false)).x();
}

milliseconds ThumbnailPanel::timeFromPosition(const QPointF& position) const
{
    return m_slider->timeFromPosition(mapToItem(m_slider, position), /*bound*/ false);
}

} // namespace nx::vms::client::desktop::workbench::timeline
