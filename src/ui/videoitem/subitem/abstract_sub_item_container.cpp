#include "abstract_sub_item_container.h"
#include "abstract_image_sub_item.h"
#include "settings.h"
#include "recording_sign_item.h"


CLAbstractSubItemContainer::CLAbstractSubItemContainer(QGraphicsItem* parent):
QGraphicsItem(parent)
{

}

CLAbstractSubItemContainer::~CLAbstractSubItemContainer()
{

}


bool CLAbstractSubItemContainer::addSubItem(CLSubItemType type)
{
	QPointF pos = getBestSubItemPos(type);
	if (pos.x()<-1000 && pos.y()<-1000)//position undefined 
		return false;

	CLAbstractSubItem* item = 0;

	switch(type)
	{
	case CloseSubItem:
		item = new CLImgSubItem(this, "./skin/close3.png" ,CloseSubItem, global_decoration_opacity, global_decoration_max_opacity, 256, 256);
		break;

	case RecordingSubItem:
		item = new CLRecordingSignItem(this);
		break;


	default:
		return false;
	}

	item->setPos(pos);

	return true;
}

void CLAbstractSubItemContainer::removeSubItem(CLSubItemType type)
{
	QList<QGraphicsItem *> childrenLst = childItems();
	foreach(QGraphicsItem * item, childrenLst)
	{
		CLAbstractSubItem* sub_item = static_cast<CLAbstractSubItem*>(item);
		if (sub_item->getType()==type)
		{
			scene()->removeItem(sub_item);
			delete sub_item;
		}

	}
}

QPointF CLAbstractSubItemContainer::getBestSubItemPos(CLSubItemType type)
{
	return QPointF(-1001,-1001);
}


void CLAbstractSubItemContainer::onResize()
{
	QList<QGraphicsItem *> childrenLst = childItems();
	foreach(QGraphicsItem * item, childrenLst)
	{
		CLAbstractSubItem* sub_item = static_cast<CLAbstractSubItem*>(item);
		QPointF pos = getBestSubItemPos(sub_item->getType());
		sub_item->setPos(pos);
		sub_item->onResize();
	}
}

void CLAbstractSubItemContainer::onSubItemPressed(CLAbstractSubItem* subitem)
{
	CLSubItemType type = subitem->getType();

	switch(type)
	{
	case CloseSubItem:
		emit onClose(this);
		break;

	default:
		break;
	}
}

void CLAbstractSubItemContainer::addToEevntTransparetList(QGraphicsItem* item)
{
	m_eventtransperent_list.push_back(item);
}

bool CLAbstractSubItemContainer::isInEventTransparetList(QGraphicsItem* item) const
{
	return m_eventtransperent_list.contains(item);
}
