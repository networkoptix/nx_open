#ifndef QN_RUBBER_BAND_INSTRUMENT_H
#define QN_RUBBER_BAND_INSTRUMENT_H

#include <QScopedPointer>
#include <QPoint>
#include <QRect>
#include <QSet>
#include "instrument.h"

class QGraphicsItem;

class RubberBandItem;

class RubberBandInstrument: public Instrument {
public:
    RubberBandInstrument(QObject *parent);
    virtual ~RubberBandInstrument();

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    static QSet<QGraphicsItem *> toSet(QList<QGraphicsItem *> items);

private:
    enum State {
        INITIAL,
        PREPAIRING,
        RUBBER_BANDING
    };

    /** Current state. */
    State mState;

    /* Scene position of the last rubber band-related mouse press event. */
    QPointF mMousePressScenePos;

    /** Widget position of the last rubber band-related mouse press event. */
    QPoint mMousePressPos;

    /* Rubber band item. */
    QScopedPointer<RubberBandItem> mRubberBand;

    /** Set of items that were selected when rubber banding has started. */
    QSet<QGraphicsItem *> mOriginallySelected;
};

#endif // QN_RUBBER_BAND_INSTRUMENT_H
