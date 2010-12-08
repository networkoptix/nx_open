#include "abstract_sub_item.h"
#include "..\abstract_scene_item.h"
#include <QGraphicsSceneMouseEvent>



CLAbstractSubItem::CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity):
CLUnMovedOpacityItem("", static_cast<QGraphicsItem*>(parent), normal_opacity, active_opacity),
m_parent(parent)
{
	setFlag(QGraphicsItem::ItemIgnoresTransformations, false); // at any momrnt this item can becom umoved; but by default disable this flag

	setAcceptsHoverEvents(true);
	//setZValue(parent->zvalue() + 1);

	connect(this, SIGNAL(onPressed(CLAbstractSubItem*)), static_cast<QObject*>(parent), SLOT(onSubItemPressed(CLAbstractSubItem*)) );
	//onSubItemPressed
}

CLAbstractSubItem::~CLAbstractSubItem()
{
	stopAnimation();
}


CLAbstractSubItem::ItemType CLAbstractSubItem::getType() const
{
	return mType;
}


void CLAbstractSubItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	emit onPressed(this);
}

void CLAbstractSubItem::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
	event->accept();
}

