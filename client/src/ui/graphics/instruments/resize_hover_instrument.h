#ifndef QN_RESIZE_HOVER_INSTURMENT_H
#define QN_RESIZE_HOVER_INSTURMENT_H

#include "instrument.h"

class FrameSectionQuearyable;

class ResizeHoverInstrument: public Instrument {
public:
    ResizeHoverInstrument(QObject *parent = NULL);

    qreal effectiveDistance() const {
        return m_effectiveDistance;
    }

    void setEffectiveDistance(qreal effectiveDistance) {
        m_effectiveDistance = effectiveDistance;
    }

    bool isEffective() const {
        return m_effective;
    }

    void setEffective(bool effective) {
        m_effective = effective;
    }

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual bool hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;
    virtual bool hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;

private:
    qreal m_effectiveDistance;
    bool m_effective;
    QHash<QGraphicsItem *, FrameSectionQuearyable *> m_queryableByItem;
};


#endif // QN_RESIZE_HOVER_INSTURMENT_H
