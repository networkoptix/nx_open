#include "abstract_unmoved_item.h"

CLAbstractUnmovedItem::CLAbstractUnmovedItem(QString name, QGraphicsItem* parent):
CLAbstractSubItemContainer(parent),
m_view(0),
m_name(name),
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

bool CLAbstractUnmovedItem::preferNonSteadyMode() const
{
    //return isUnderMouse(); //qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
    return m_underMouse;
}

void CLAbstractUnmovedItem::hoverEnterEvent(QGraphicsSceneHoverEvent */*event*/)
{
    m_underMouse = true;
}

void CLAbstractUnmovedItem::hoverLeaveEvent(QGraphicsSceneHoverEvent */*event*/)
{
    m_underMouse = false;
}

void CLAbstractUnmovedItem::setStaticPos(QPoint p)
{
	m_pos = p;
}

void CLAbstractUnmovedItem::adjust()
{
	if (!m_view)
		m_view = scene()->views().at(0);

	setPos(m_view->mapToScene(m_pos));
}

QString CLAbstractUnmovedItem::getName() const
{
	return m_name;
}
