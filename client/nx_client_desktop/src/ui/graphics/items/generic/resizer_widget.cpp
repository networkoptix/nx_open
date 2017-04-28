#include "resizer_widget.h"

#include <QtGui/QPainter>

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

/* This method will never be called while item has ItemHasNoContents flag and exists solely for
 * debugging purposes. */
void QnResizerWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
    QWidget* /*widget*/)
{
    painter->fillRect(option->rect, Qt::darkMagenta);
}
