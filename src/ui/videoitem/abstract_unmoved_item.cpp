#include "abstract_unmoved_item.h"
#include <QGraphicsView>


CLAbstractUnmovedItem::CLAbstractUnmovedItem(QGraphicsView* view, QString name):
m_view(view),
m_name(name)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations);
	//setFlag(QGraphicsItem::ItemIsMovable);

}

CLAbstractUnmovedItem::~CLAbstractUnmovedItem()
{

}

void CLAbstractUnmovedItem::setStaticPos(QPoint p)
{
	m_pos = p;
}


void CLAbstractUnmovedItem::adjust()
{
	setPos(m_view->mapToScene(m_pos));
}

QString CLAbstractUnmovedItem::getName() const
{
	return m_name;
}