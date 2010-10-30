#ifndef view_drag_and_drop_helper_h_1835
#define view_drag_and_drop_helper_h_1835

#include <QList>
#include <QDataStream>

class QGraphicsItem;

struct CLDragAndDropItems
{
	QList<int> layoutlinks;
	QList<QString> videodevices;
	QList<QString> recorders;

	bool isEmpty()
	{
		return layoutlinks.count() == 0 && videodevices.count() == 0 && recorders.count() == 0;
	}
};

void items2DDstream(QList<QGraphicsItem*> itemslst, QDataStream& dataStream);

void DDstream2items(QDataStream& dataStream, CLDragAndDropItems& items);

#endif //view_drag_and_drop_helper_h_1835
