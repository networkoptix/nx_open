#ifndef QN_HAND_SCROLL_INSTRUMENT_H
#define QN_HAND_SCROLL_INSTRUMENT_H

#include <QCursor>
#include "instrument.h"

class HandScrollInstrument: public Instrument {
    Q_OBJECT;
public:
    HandScrollInstrument(QObject *parent);

protected:
    virtual void installedNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;

private:
    enum State {
        INITIAL,
        PREPAIRING,
        SCROLLING
    };

    State m_state;
    QPoint m_mousePressPos;
    QPoint m_lastMousePos;
    QCursor m_originalCursor;
};

#endif // QN_HAND_SCROLL_INSTRUMENT_H
