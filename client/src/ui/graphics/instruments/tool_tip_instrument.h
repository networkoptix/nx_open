#ifndef QN_TOOL_TIP_INSTRUMENT_H
#define QN_TOOL_TIP_INSTRUMENT_H

#include "instrument.h"

/**
 * Instrument that implements tooltips that are shown on the scene as graphics items. 
 * This approach is better than the default one because showing widgets on top of 
 * OpenGL window (graphics view in our case) results in FPS drop.
 */
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
