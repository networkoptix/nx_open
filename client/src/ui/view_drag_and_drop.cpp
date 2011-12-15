#include "view_drag_and_drop.h"

#include "videoitem/abstract_scene_item.h"
#include "videoitem/video_wnd_item.h"
#include "camera/camera.h"
#include "videoitem/recorder_item.h"
#include "videoitem/layout_item.h"
#include "core/resourcemanagment/resource_pool.h"

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
			items.layoutlinks.push_back((int)(long)it->getRefContent());
		}
	}

	dataStream << items.videodevices << items.recorders << items.layoutlinks;
}

void DDstream2items(QDataStream& dataStream, CLDragAndDropItems& items)
{
	dataStream >> items.videodevices >> items.recorders >> items.layoutlinks;
}


namespace {
    enum {
        RESOURCES_BINARY_V1_TAG = 0xE1E00001
    };

    quint64 localMagic = QDateTime::currentMSecsSinceEpoch();

} // anonymous namespace

QString resourcesMime() {
    return QLatin1String("application/x-noptix-resources");
}

QByteArray serializeResources(const QnResourceList &resources) {
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    /* Magic signature. */
    stream << static_cast<quint32>(RESOURCES_BINARY_V1_TAG);
    
    /* For local D&D. */
    stream << static_cast<quint64>(QApplication::applicationPid());
    stream << static_cast<quint64>(localMagic);

    /* Data. */
    stream << static_cast<quint32>(resources.size());
    foreach(const QnResourcePtr &resource, resources)
        stream << resource->getId().toString();

    return result;
}

QnResourceList deserializeResources(const QByteArray &data) {
    QByteArray tmp = data;
    QDataStream stream(&tmp, QIODevice::ReadOnly);
    QnResourceList result;

    quint32 tag;
    stream >> tag;
    if(tag != RESOURCES_BINARY_V1_TAG)
        return result;

    quint64 pid;
    stream >> pid;
    if(pid != QApplication::applicationPid())
        return result; // TODO: originated from another application.

    quint64 magic;
    stream >> magic;
    if(magic != localMagic)
        return result; // TODO: originated from another application.

    quint32 size;
    stream >> size;
    for(int i = 0; i < size; i++) {
        QString id;
        stream >> id;

        QnResourcePtr resource = qnResPool->getResourceById(id);
        if(!resource.isNull())
            result.push_back(resource);
    }

    return result;
}
