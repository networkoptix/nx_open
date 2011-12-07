#ifndef CLUnMovedInteractiveOpacityItem_1758_h
#define CLUnMovedInteractiveOpacityItem_1758_h

#include "abstract_unmoved_item.h"

class CLUnMovedInteractiveOpacityItem : public CLAbstractUnmovedItem
{
    Q_OBJECT

public:
    CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
    ~CLUnMovedInteractiveOpacityItem();

    virtual void setVisible(bool visible, int duration = 0);

protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

protected:
    const qreal m_normal_opacity;
    const qreal m_active_opacity;
};

#endif //CLUnMovedInteractiveOpacityItem
