#ifndef abstract_sub_item_container_h1031
#define abstract_sub_item_container_h1031

#include <QGraphicsItem>
#include <QObject>

class CLAbstractSubItem;

enum CLSubItemType {CloseSubItem, ArchiveNavigatorSubItem, RecordingSubItem, PlaySubItem, PauseSubItem, StepForwardSubItem, StepBackwardSubItem, RewindBackwardSubItem, RewindForwardSubItem};

class CLAbstractSubItemContainer : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	CLAbstractSubItemContainer(QGraphicsItem* parent);

	~CLAbstractSubItemContainer();


	bool addSubItem(CLSubItemType type);
	virtual void removeSubItem(CLSubItemType type);
	virtual QPointF getBestSubItemPos(CLSubItemType type);

signals:
	void onClose(CLAbstractSubItemContainer*);

public slots:
	void onResize();

protected slots:
	virtual void onSubItemPressed(CLAbstractSubItem* subitem);

	

};


#endif //abstract_sub_item_container_h1031