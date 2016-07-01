#ifndef QN_TRANSFORM_LISTENER_INSTRUMENT_H
#define QN_TRANSFORM_LISTENER_INSTRUMENT_H

#include "instrument.h"
#include <QHash>
#include <QtGui/QTransform>

/**
 * This instrument listens to view transformation and size changes and notifies 
 * about them via <tt>transformChanged</tt> and <tt>sizeChanged</tt> signals.
 */
class TransformListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    TransformListenerInstrument(QObject *parent = NULL);

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
