#ifndef CLUnMovedInteractiveOpacityItem_1758_h
#define CLUnMovedInteractiveOpacityItem_1758_h

#include "abstract_unmoved_opacity_item.h"
class QPropertyAnimation;


class CLUnMovedInteractiveOpacityItem: public CLAbstractUnMovedOpacityItem
{
	Q_OBJECT
public:
	CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
	~CLUnMovedInteractiveOpacityItem();

    virtual void hide(int duration);
    virtual void show(int duration);


protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);



protected:
    bool needAnimation() const;
protected:
	qreal m_normal_opacity;
	qreal m_active_opacity;
};


#endif //CLUnMovedInteractiveOpacityItem
