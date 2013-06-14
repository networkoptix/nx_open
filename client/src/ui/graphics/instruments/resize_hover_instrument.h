#ifndef QN_RESIZE_HOVER_INSTURMENT_H
#define QN_RESIZE_HOVER_INSTURMENT_H

#include "instrument.h"

class FrameSectionQueryable;

class ResizeHoverInstrument: public Instrument {
public:
    ResizeHoverInstrument(QObject *parent = NULL);

    qreal effectRadius() const {
        return m_effectRadius;
    }

    void setEffectRadius(qreal effectRadius) {
        m_effectRadius = effectRadius;
    }

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual bool hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;
    virtual bool hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;

private:
    qreal m_effectRadius;
    QHash<QGraphicsItem *, FrameSectionQueryable *> m_queryableByItem;
    QSet<QGraphicsItem *> m_affectedItems;
};


#endif // QN_RESIZE_HOVER_INSTURMENT_H
