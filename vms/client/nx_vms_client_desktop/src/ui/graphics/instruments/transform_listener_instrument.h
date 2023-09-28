// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_TRANSFORM_LISTENER_INSTRUMENT_H
#define QN_TRANSFORM_LISTENER_INSTRUMENT_H

#include <QHash>

#include <QtGui/QTransform>

#include "instrument.h"

/**
 * This instrument listens to view transformation and size changes and notifies
 * about them via <tt>transformChanged</tt> and <tt>sizeChanged</tt> signals.
 */
class TransformListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    TransformListenerInstrument(QObject *parent = nullptr);

signals:
    void transformChanged(QGraphicsView *view);
    void sizeChanged(QGraphicsView *view);

protected:
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;
    virtual bool resizeEvent(QWidget *viewport, QResizeEvent *event) override;

private:
    QHash<QGraphicsView *, QTransform> m_transforms;
};

#endif // QN_TRANSFORM_LISTENER_INSTRUMENT_H
