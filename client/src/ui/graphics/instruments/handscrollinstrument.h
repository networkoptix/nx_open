#ifndef QN_HAND_SCROLL_INSTRUMENT_H
#define QN_HAND_SCROLL_INSTRUMENT_H

#include <QCursor>
#include "dragprocessinginstrument.h"

class HandScrollInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    HandScrollInstrument(QObject *parent);
    virtual ~HandScrollInstrument();

signals:
    void scrollingStarted(QGraphicsView *view);
    void scrollingFinished(QGraphicsView *view);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDrag() override;
    virtual void drag() override;
    virtual void finishDrag() override;

private:
    QCursor m_originalCursor;
};

#endif // QN_HAND_SCROLL_INSTRUMENT_H
