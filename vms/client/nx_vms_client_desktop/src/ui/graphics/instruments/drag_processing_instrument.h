#ifndef QN_DRAG_PROCESSING_INSTRUMENT_H
#define QN_DRAG_PROCESSING_INSTRUMENT_H

#include "instrument.h"
#include <ui/processors/drag_processor.h>

/**
 * This instrument sets up drag processor on construction and implements default
 * event forwarding. In case non-default event forwarding is desired, appropriate
 * event handlers can be overridden.
 */
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

public slots:
    /**
     * Resets the drag process.
     */
    void reset();

    void resetLater();

protected:
    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseDoubleClickEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseMoveEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mouseReleaseEvent(QWidget* viewport, QMouseEvent* event) override;

    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

    virtual bool mousePressEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseMoveEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseReleaseEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event) override;

    virtual bool mousePressEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseMoveEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;

private:
    void initialize();
};

#endif // QN_DRAG_PROCESSING_INSTRUMENT_H
