#ifndef QN_FOCUS_LISTENER_INSTRUMENT_H
#define QN_FOCUS_LISTENER_INSTRUMENT_H

#include "instrument.h"

class FocusListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    FocusListenerInstrument(QObject *parent = NULL);
    virtual ~FocusListenerInstrument();

signals:
    void focusItemChanged();

protected:
    virtual bool animationEvent(AnimationEvent *event) override;

    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

private:
    void setFocusItem(QGraphicsItem *focusItem);

private:
    /** Item that currently has focus, with a one-frame precision. 
     * 
     * Note that there is no getter for this item as it may be destroyed,
     * and this instrument doesn't take care of clearing the pointer in this case. */
    QGraphicsItem *m_focusItem;
};


#endif // QN_FOCUS_LISTENER_INSTRUMENT_H
