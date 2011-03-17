#include "view_drag_and_drop.h"

#include "videoitem\abstract_scene_item.h"
#include "videoitem\video_wnd_item.h"
#include "camera\camera.h"
#include "device\device.h"
#include "videoitem\recorder_item.h"
#include "videoitem\layout_item.h"

class QGraphicsItem;

void items2DDstream(QList<QGraphicsItem*> itemslst, QDataStream& dataStream)
{

	CLDragAndDropItems items;

	foreach(QGraphicsItem* itm, itemslst)
	{
		CLAbstractSceneItem* item = static_cast<CLAbstractSceneItem*>(itm);

		CLAbstractSceneItem::CLSceneItemType type = item->getType();

		//{VIDEO, IMAGE, BUTTON, RECORDER, LAYOUT};
		if (type == CLAbstractSceneItem::VIDEO)
		{
			CLVideoWindowItem* it = static_cast<CLVideoWindowItem*>(item);
			items.videodevices.push_back(it->getComplicatedItem()->getDevice()->getUniqueId());
		}
		else if (type == CLAbstractSceneItem::RECORDER)
		{
			CLRecorderItem* it = static_cast<CLRecorderItem*>(item);
			items.recorders.push_back(it->getName());
		}
		else if (type == CLAbstractSceneItem::LAYOUT)
		{
			CLLayoutItem* it = static_cast<CLLayoutItem*>(item);
			items.layoutlinks.push_back((int)it->getRefContent());
		}
	}

	dataStream << items.videodevices << items.recorders << items.layoutlinks;
}

void DDstream2items(QDataStream& dataStream, CLDragAndDropItems& items)
{
	dataStream >> items.videodevices >> items.recorders >> items.layoutlinks;
}