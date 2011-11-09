#ifndef QN_TRANSFORM_LISTENER_INSTRUMENT_H
#define QN_TRANSFORM_LISTENER_INSTRUMENT_H

#include "instrument.h"
#include <QHash>
#include <QTransform>

class TransformListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    TransformListenerInstrument(QObject *parent = NULL);

signals:
    void transformChanged(QGraphicsView *view);

protected:
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    QHash<QGraphicsView *, QTransform> mTransforms;
};

#endif // QN_TRANSFORM_LISTENER_INSTRUMENT_H
