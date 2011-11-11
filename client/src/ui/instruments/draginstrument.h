#ifndef QN_DRAG_INSTRUMENT_H
#define QN_DRAG_INSTRUMENT_H

#include <QPoint>
#include "instrument.h"

class DragInstrument: public Instrument {
    Q_OBJECT;
public:
    DragInstrument(QObject *parent);
    virtual ~DragInstrument();

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;

signals:
    void draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

private:
    void startDragging(QGraphicsView *view);
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
    QWeakPointer<QGraphicsView> m_view;
    QGraphicsItem *m_itemToSelect;
};

#endif // QN_DRAG_INSTRUMENT_H
