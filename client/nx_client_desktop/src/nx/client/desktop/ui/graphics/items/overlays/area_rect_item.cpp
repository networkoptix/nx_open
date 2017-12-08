#include "area_rect_item.h"

namespace nx {
namespace client {
namespace desktop {

AreaRectItem::AreaRectItem(QGraphicsItem* parentItem, QObject* parentObject):
    QObject(parentObject),
    GraphicsRectItem(parentItem)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(true);
}

bool AreaRectItem::containsMouse() const
{
    return m_containsMouse;
}

void AreaRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* /*event*/)
{
    m_containsMouse = true;
    emit containsMouseChanged(m_containsMouse);
}

void AreaRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* /*event*/)
{
    m_containsMouse = false;
    emit containsMouseChanged(m_containsMouse);
}

} // namespace desktop
} // namespace client
} // namespace nx
