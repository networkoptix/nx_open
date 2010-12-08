#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include <QGraphicsItem>
#include "..\unmoved\abstract_animated_unmoved_item.h"

class CLAbstractSceneItem;
class CLAbstractSubItem;

class CLAbstractSubItem : public CLUnMovedOpacityItem
{
	Q_OBJECT
public:
	enum ItemType {Close, ArchiveNavigator, Recording, Play, Pause, StepForward, StepBackward, RewindBackward, RewindForward};
	CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity);
	virtual ~CLAbstractSubItem();

	virtual ItemType getType() const;

	virtual void onResize() = 0;

signals:
	void onPressed(CLAbstractSubItem*);
protected:
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected slots:
protected:
	ItemType mType;
	CLAbstractSubItemContainer* m_parent;
	

};

#endif //abstract_sub_item_h1031