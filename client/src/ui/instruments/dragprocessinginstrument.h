#ifndef QN_DRAG_PROCESSING_INSTRUMENT_H
#define QN_DRAG_PROCESSING_INSTRUMENT_H

#include "instrument.h"
#include "dragprocessor.h"

class DragProcessingInstrument: public Instrument, public DragProcessHandler {
    Q_OBJECT;
public:
    DragProcessingInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {
        initialize();
    }

    DragProcessingInstrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent = NULL):
        Instrument(type, eventTypes, parent)
    {
        initialize();
    }

protected:
    virtual void aboutToBeUninstalledNotify() override {
        Instrument::aboutToBeUninstalledNotify();

        processor()->reset();
    }

    virtual void aboutToBeDisabledNotify() override {
        Instrument::aboutToBeDisabledNotify();

        processor()->reset();
    }

private:
    void initialize() {
        DragProcessor *processor = new DragProcessor(this);
        processor->setHandler(this);
    }
};


#endif // QN_DRAG_PROCESSING_INSTRUMENT_H
