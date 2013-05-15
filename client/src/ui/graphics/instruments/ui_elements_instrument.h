#ifndef QN_UI_ELEMENTS_INSTRUMENT_H
#define QN_UI_ELEMENTS_INSTRUMENT_H

#include "instrument.h"

#include <QtCore/QWeakPointer>

class QGraphicsWidget;
class DestructionGuardItem;


/**
 * Note that even though this instrument works fine with multiple views, 
 * it is not suited to be used like that.
 */
class UiElementsInstrument: public Instrument {
    Q_OBJECT;
public:
    UiElementsInstrument(QObject *parent);
    virtual ~UiElementsInstrument();

    /**
     * \returns                         Widget that this instrument works with.
     *                                  Is never NULL, unless explicitly deleted by user.
     */
    QGraphicsWidget *widget() const {
        return m_widget.data();
    }

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    DestructionGuardItem *guard() const {
        return m_guard.data();
    }

private:
    QWeakPointer<DestructionGuardItem> m_guard;
    QWeakPointer<QGraphicsWidget> m_widget;
};


#endif // QN_UI_ELEMENTS_INSTRUMENT_H
