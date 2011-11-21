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

public slots:
    void adjustPosition(QGraphicsView *view);

protected:
    virtual void installedNotify();
    virtual void uninstalledNotify();

    virtual void resizeEvent(QWidget *viewport, QResizeEvent *event);

private:
    QWeakPointer<QGraphicsWidget> m_widget;
};


#endif // QN_UI_ELEMENTS_INSTRUMENT_H
