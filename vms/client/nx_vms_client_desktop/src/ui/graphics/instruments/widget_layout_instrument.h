#ifndef QN_WIDGET_LAYOUT_INSTRUMENT
#define QN_WIDGET_LAYOUT_INSTRUMENT

#include "instrument.h"

class WidgetLayoutInstrument: public Instrument {
    Q_OBJECT;
public:
    WidgetLayoutInstrument(QObject *parent = NULL);
    virtual ~WidgetLayoutInstrument();

protected:
    virtual bool animationEvent(AnimationEvent *event) override;
};

#endif // QN_WIDGET_LAYOUT_INSTRUMENT
