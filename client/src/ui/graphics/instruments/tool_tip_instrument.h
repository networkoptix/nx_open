#ifndef QN_TOOL_TIP_INSTRUMENT_H
#define QN_TOOL_TIP_INSTRUMENT_H

#include "instrument.h"

class ToolTipInstrument: public Instrument {
    Q_OBJECT

    typedef Instrument base_type;

public:
    ToolTipInstrument(QObject *parent);
    virtual ~ToolTipInstrument();

protected:
    virtual bool event(QWidget *viewport, QEvent *event) override;
};

#endif // QN_TOOL_TIP_INSTRUMENT_H
