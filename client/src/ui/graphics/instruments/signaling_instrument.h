#ifndef QN_SIGNALING_INSTRUMENT_H
#define QN_SIGNALING_INSTRUMENT_H

#include "instrument.h"

class SignalingInstrument: public Instrument {
    Q_OBJECT;
public:
    SignalingInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent),
        m_previousTime(0)
    {}

    SignalingInstrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent = NULL):
        Instrument(type, eventTypes, parent),
        m_previousTime(0)
    {}

signals:
    void activated(QGraphicsScene *scene, QEvent *event);
    void activated(QGraphicsView *view, QEvent *event);
    void activated(QWidget *viewport, QEvent *event);
    void activated(QGraphicsItem *item, QEvent *event);

protected:
    virtual bool event(QGraphicsScene *scene, QEvent *event) override {
        emit activated(scene, event);
        return false;
    }

    virtual bool event(QGraphicsView *view, QEvent *event) override {
        emit activated(view, event);
        return false;
    }

    virtual bool event(QWidget *viewport, QEvent *event) override {
        emit activated(viewport, event);
        return false;
    }

    virtual bool sceneEvent(QGraphicsItem *item, QEvent *event) override {
        emit activated(item, event);
        return false;
    }
private:
    quint64 m_previousTime;
};

#endif // QN_SIGNALING_INSTRUMENT_H
