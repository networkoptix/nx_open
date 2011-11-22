#ifndef QN_UI_ELEMENTS_INSTRUMENT_H
#define QN_UI_ELEMENTS_INSTRUMENT_H

#include "instrument.h"
#include <QWeakPointer>

class QGraphicsWidget;

/**
 * Note that even though this instrument works with multiple views, it is not
 * suited to be used like that.
 */
class UiElementsInstrument: public Instrument {
    Q_OBJECT;
public:
    UiElementsInstrument(QObject *parent);
    virtual ~UiElementsInstrument();

protected:
    virtual void installedNotify();
    virtual void uninstalledNotify();

    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    QWeakPointer<QGraphicsWidget> m_widget;
};


#endif // QN_UI_ELEMENTS_INSTRUMENT_H
