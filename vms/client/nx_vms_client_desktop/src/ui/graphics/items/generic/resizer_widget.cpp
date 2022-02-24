// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resizer_widget.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsSceneEvent>
#include <QtWidgets/QStyleOptionGraphicsItem>

QnResizerWidget::QnResizerWidget(Qt::Orientation orientation, QGraphicsItem *parent,
    Qt::WindowFlags wFlags)
    :
    base_type(parent, wFlags),
    m_orientation(orientation)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(false);
    setFlag(ItemHasNoContents, true);
    setHandlingFlag(ItemHandlesMovement, true);
    setEnabled(true);
}

void QnResizerWidget::setEnabled(bool enabled)
{
    setFlag(ItemIsMovable, enabled);
    if (enabled)
        setCursor(m_orientation == Qt::Vertical ? Qt::SizeVerCursor : Qt::SizeHorCursor);
    else
        setCursor(QCursor());
}

bool QnResizerWidget::isBeingMoved() const
{
    return m_isBeingMoved;
}

/* This method will never be called while item has ItemHasNoContents flag and exists solely for
 * debugging purposes. */
void QnResizerWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
    QWidget* /*widget*/)
{
    painter->fillRect(option->rect, Qt::darkMagenta);
}

void QnResizerWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Resizer should be movable, but some flags could be changed externally.
    if (event->buttons().testFlag(Qt::LeftButton)
        && flags().testFlag(ItemIsMovable)
        && handlingFlags().testFlag(ItemHandlesMovement))
    {
        m_isBeingMoved = true;
    }

    base_type::mousePressEvent(event);
}

void QnResizerWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // The base class doesn't stop move processing until all buttons are released.
    if (!event->buttons())
        m_isBeingMoved = false;

    base_type::mouseReleaseEvent(event);
}
