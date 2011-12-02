#ifndef QN_HAND_SCROLL_INSTRUMENT_H
#define QN_HAND_SCROLL_INSTRUMENT_H

#include <QCursor>
#include <ui\processors\kineticprocesshandler.h>
#include "dragprocessinginstrument.h"

class HandScrollInstrument: public DragProcessingInstrument, protected KineticProcessHandler {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;

public:
    HandScrollInstrument(QObject *parent);
    virtual ~HandScrollInstrument();

signals:
    void scrollProcessStarted(QGraphicsView *view);
    void scrollStarted(QGraphicsView *view);
    void scrollFinished(QGraphicsView *view);
    void scrollProcessFinished(QGraphicsView *view);

protected:
    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

    virtual void kineticMove(const QVariant &distance) override;
    virtual void finishKinetic() override;

private:
    QCursor m_originalCursor;
    QWeakPointer<QGraphicsView> m_currentView;
};

#endif // QN_HAND_SCROLL_INSTRUMENT_H
