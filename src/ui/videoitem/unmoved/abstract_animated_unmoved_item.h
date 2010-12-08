#ifndef abstruct_unmoved_animated_item_1758_h
#define abstruct_unmoved_animated_item_1758_h

#include "abstract_unmoved_item.h"
class QPropertyAnimation;

class CLUnMovedOpacityItem: public CLAbstractUnmovedItem
{
	Q_OBJECT
	Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
	CLUnMovedOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
	~CLUnMovedOpacityItem();
protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
protected slots:
	void stopAnimation();
	bool needAnimation() const;
protected:
	qreal m_normal_opacity;
	qreal m_active_opacity;
	QPropertyAnimation* m_animation;
};


#endif //abstruct_unmoved_animated_item_1758_h
