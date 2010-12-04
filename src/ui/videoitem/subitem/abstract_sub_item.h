#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include <QGraphicsItem>
#include "..\abstract_unmoved_item.h"

class CLAbstractSceneItem;
class QPropertyAnimation;
class CLAbstractSubItem;



class CLAbstractSubItem : public CLAbstractUnmovedItem
{
	Q_OBJECT
	Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
	enum ItemType {Close, ArchiveNavigator, Recording, Play, Pause, StepForward, StepBackward, RewindBackward, RewindForward};
	CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal opacity, qreal max_opacity);
	virtual ~CLAbstractSubItem();

	virtual ItemType getType() const;

	virtual void onResize() = 0;

signals:
	void onPressed(CLAbstractSubItem*);
protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected slots:
	void stopAnimation();
	bool needAnimation() const;
protected:
	qreal m_opacity, m_maxopacity;
	QPropertyAnimation* m_animation;
	ItemType mType;
	CLAbstractSubItemContainer* m_parent;
	

};

#endif //abstract_sub_item_h1031