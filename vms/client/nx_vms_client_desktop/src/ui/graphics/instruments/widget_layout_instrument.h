// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WIDGET_LAYOUT_INSTRUMENT
#define QN_WIDGET_LAYOUT_INSTRUMENT

#include "instrument.h"

class WidgetLayoutInstrument: public Instrument {
    Q_OBJECT;
public:
    WidgetLayoutInstrument(QObject *parent = nullptr);
    virtual ~WidgetLayoutInstrument();

protected:
    virtual bool animationEvent(AnimationEvent *event) override;
};

#endif // QN_WIDGET_LAYOUT_INSTRUMENT
