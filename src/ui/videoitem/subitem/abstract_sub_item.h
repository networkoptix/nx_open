#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include "..\unmoved\unmoved_interactive_opacity_item.h"

class CLAbstractSceneItem;
class CLAbstractSubItem;

class CLAbstractSubItem : public CLUnMovedInteractiveOpacityItem
{
	Q_OBJECT
public:
	CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity);
	virtual ~CLAbstractSubItem();

	virtual CLSubItemType getType() const;

	virtual void onResize() = 0;

signals:
	void onPressed(CLAbstractSubItem*);
protected:
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected slots:
protected:
	CLSubItemType mType;
	CLAbstractSubItemContainer* m_parent;

};

#endif //abstract_sub_item_h1031