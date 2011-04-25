#include "abstract_unmoved_item.h"

CLAbstractUnmovedItem::CLAbstractUnmovedItem(QString name, QGraphicsItem* parent):
CLAbstractSubItemContainer(parent),
m_view(0),
m_name(name)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations);
	//setFlag(QGraphicsItem::ItemIsMovable);

}

CLAbstractUnmovedItem::~CLAbstractUnmovedItem()
{

}

void CLAbstractUnmovedItem::hideIfNeeded(int duration)
{
    hide(duration);
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
