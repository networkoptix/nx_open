#ifndef CLAbstractUnMovedOpacityItem_1758_h
#define CLAbstractUnMovedOpacityItem_1758_h

#include "abstract_unmoved_item.h"
class QPropertyAnimation;


class CLAbstractUnMovedOpacityItem: public CLAbstractUnmovedItem
{
	Q_OBJECT
	Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
	CLAbstractUnMovedOpacityItem(QString name, QGraphicsItem* parent);
	~CLAbstractUnMovedOpacityItem();
protected:
    void changeOpacity(qreal new_opacity, int duration_ms);
protected slots:
	void stopAnimation();
protected:

	QPropertyAnimation* m_animation;
};


#endif //CLAbstractUnMovedOpacityItem_1758
