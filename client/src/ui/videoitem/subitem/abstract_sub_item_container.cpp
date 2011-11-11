#include "abstract_sub_item_container.h"
#include "abstract_image_sub_item.h"
#include "settings.h"
#include "recording_sign_item.h"
#include "ui/skin.h"

CLAbstractSubItemContainer::CLAbstractSubItemContainer(QGraphicsItem* parent):
QGraphicsItem(parent)
{

}

CLAbstractSubItemContainer::~CLAbstractSubItemContainer()
{

}

QList<CLAbstractSubItem*> CLAbstractSubItemContainer::subItemList() const
{
    return m_subItems;
}

void CLAbstractSubItemContainer::addSubItem(CLAbstractSubItem *item)
{
    if (!m_subItems.contains(item))
        m_subItems.push_back(item);
}

void CLAbstractSubItemContainer::removeSubItem(CLAbstractSubItem *item)
{
    m_subItems.removeOne(item);
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
		item = new CLImgSubItem(this, Skin::path(QLatin1String("close3.png")), type, global_decoration_opacity, global_decoration_max_opacity, 300, 300);
		break;

    case MakeScreenshotSubItem:
        item = new CLImgSubItem(this, Skin::path(QLatin1String("camera.png")), type, global_decoration_opacity, global_decoration_max_opacity, 300, 300);
        break;

	case RecordingSubItem:
		item = new CLRecordingSignItem(this);
		break;

	default:
		return false;
	}

	item->setPos(pos);

	addSubItem(item);

	return true;
}

void CLAbstractSubItemContainer::removeSubItem(CLSubItemType type)
{
	foreach(CLAbstractSubItem* sub_item, m_subItems)
	{
		if (sub_item->getType()==type)
		{
			removeSubItem(sub_item);
			scene()->removeItem(sub_item);
			delete sub_item;
			break;
		}

	}
}

QPointF CLAbstractSubItemContainer::getBestSubItemPos(CLSubItemType /*type*/)
{
	return QPointF(-1001,-1001);
}

void CLAbstractSubItemContainer::onResize()
{
    foreach(CLAbstractSubItem* sub_item, m_subItems)
    {
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

	case MakeScreenshotSubItem:
		emit onMakeScreenshot(this);
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
