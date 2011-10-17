#ifndef QN_DRAG_INSTRUMENT_H
#define QN_DRAG_INSTRUMENT_H

#include <QPoint>
#include "instrument.h"

class DragInstrument: public Instrument {
    Q_OBJECT;
public:
    DragInstrument(QObject *parent);

protected:
    virtual void installedNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;

signals:
    void draggingStarted(QList<QGraphicsItem *> items);
    void draggingFinished(QList<QGraphicsItem *> items);

private:
    void startDragging();
    void stopDragging();

private:
    enum State {
        INITIAL,
        PREPAIRING,
        DRAGGING
    };

    State m_state;
    QPoint m_mousePressPos;
    QPointF m_lastMouseScenePos;
};

#endif // QN_DRAG_INSTRUMENT_H
