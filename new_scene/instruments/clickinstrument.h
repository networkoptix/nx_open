#ifndef QN_CLICK_INSTRUMENT_H
#define QN_CLICK_INSTRUMENT_H

#include "instrument.h"
#include <QPoint>

class ClickInstrument: public Instrument {
    Q_OBJECT;
public:
    ClickInstrument(QObject *parent = NULL);

signals:
    void clicked(QGraphicsView *view, QGraphicsItem *item);
    void doubleClicked(QGraphicsView *view, QGraphicsItem *item);

protected:
    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);
    virtual bool mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event);

    virtual bool isWillingToWatch(QGraphicsItem *) const override { return true; }

private:
    QPoint m_mousePressPos;
    bool m_isClick;
    bool m_isDoubleClick;
};

#endif // QN_CLICK_INSTRUMENT_H
