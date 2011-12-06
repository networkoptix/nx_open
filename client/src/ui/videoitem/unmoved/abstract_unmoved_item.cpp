#include "abstract_unmoved_item.h"

CLAbstractUnmovedItem::CLAbstractUnmovedItem(QString name, QGraphicsItem* parent) :
    CLAbstractSubItemContainer(parent),
    m_name(name),
    m_view(0),
    m_underMouse(false)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    //setFlag(QGraphicsItem::ItemIsMovable);

    setAcceptsHoverEvents(true);
}

CLAbstractUnmovedItem::~CLAbstractUnmovedItem()
{
}

void CLAbstractUnmovedItem::hideIfNeeded(int duration)
{
    hide(duration);
}

bool CLAbstractUnmovedItem::mouseOver() const
{
    //return isUnderMouse(); //qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
    return m_underMouse;
}

void CLAbstractUnmovedItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractSubItemContainer::hoverEnterEvent(event);
    m_underMouse = true;
}

void CLAbstractUnmovedItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    CLAbstractSubItemContainer::hoverLeaveEvent(event);
    m_underMouse = false;
}

void CLAbstractUnmovedItem::setStaticPos(const QPoint &p)
{
    m_pos = p;
}

void CLAbstractUnmovedItem::adjust()
{
    if (!scene())
        return;

    if (!m_view)
        m_view = scene()->views().at(0);

    setPos(m_view->mapToScene(m_pos));
}

QString CLAbstractUnmovedItem::getName() const
{
    return m_name;
}
