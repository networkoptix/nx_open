#ifndef QN_HAND_SCROLL_INSTRUMENT_H
#define QN_HAND_SCROLL_INSTRUMENT_H

#include <QCursor>
#include <ui\processors\kineticprocessor.h>
#include "dragprocessinginstrument.h"

class HandScrollInstrument: public DragProcessingInstrument, protected KineticProcessHandler<QPointF> {
    Q_OBJECT;
public:
    HandScrollInstrument(QObject *parent);
    virtual ~HandScrollInstrument();

signals:
    void scrollProcessStarted(QGraphicsView *view);
    void scrollStarted(QGraphicsView *view);
    void scrollFinished(QGraphicsView *view);
    void scrollProcessFinished(QGraphicsView *view);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

    virtual void kineticMove(const QPointF &distance) override;
    virtual void finishKinetic() override;

private:
    QCursor m_originalCursor;
    QWeakPointer<QGraphicsView> m_currentView;
};

#endif // QN_HAND_SCROLL_INSTRUMENT_H
