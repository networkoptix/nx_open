#include "abstract_sub_item.h"
#include "..\abstract_scene_item.h"

CLAbstractSubItem::CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity):
CLUnMovedInteractiveOpacityItem("", static_cast<QGraphicsItem*>(parent), normal_opacity, active_opacity),
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

CLSubItemType CLAbstractSubItem::getType() const
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

