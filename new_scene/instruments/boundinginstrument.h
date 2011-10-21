#ifndef QN_BOUNDING_INSTRUMENT_H
#define QN_BOUNDING_INSTRUMENT_H

#include "instrument.h"
#include <QRect>
#include <QTransform>
#include "animationtimer.h"

class BoundingInstrument: public Instrument, protected AnimationTimerListener {
    Q_OBJECT;
public:
    BoundingInstrument(QObject *parent = NULL);

    virtual ~BoundingInstrument();

    /**
     * \param positionBounds            Rectangular area to which viewport movement is restricted, in scene coordinates.
     * \param extension                 Extension of this area, in viewports.
     */
    void setPositionBounds(QGraphicsView *view, const QRectF &positionBounds, qreal extension);

    /**
     * \param sizeBound                 Size to which viewport scaling is restricted, in scene coordinates.
     */
    void setSizeBounds(QGraphicsView *view, const QSizeF &sizeBound);

    /**
     * \param multiplier                Viewport movement speed, in viewports per second.
     */
    void setMovementSpeed(QGraphicsView *view, qreal multiplier);

    /**
     * \param multiplier                Scale speed, factor per second.
     */
    void setScalingSpeed(QGraphicsView *view, qreal multiplier);

    /**
     * \param positionEnforced          Whether position boundary is enforced with animation.
     */
    void setPositionEnforced(QGraphicsView *view, bool positionEnforced = true);

    /**
     * \param sizeEnforced              Whether size boundary is enforced with animation.
     */
    void setSizeEnforced(QGraphicsView *view, bool sizeEnforced = true);

public slots:
    void enforcePosition(QGraphicsView *view);
    void dontEnforcePosition(QGraphicsView *view);
    void enforceSize(QGraphicsView *view);
    void dontEnforceSize(QGraphicsView *view);

protected:
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void tick(int currentTime) override;

private:
    class ViewData;

    ViewData *data(QGraphicsView *view);
    ViewData *cdata(QGraphicsView *view) const;

private:
    /** View to data mapping. */
    QHash<QGraphicsView *, ViewData *> m_data;

    /** Animation timer. */
    AnimationTimer *m_timer;

    /** Last animation tick time. */
    int m_lastTickTime;
};

#endif // QN_BOUNDING_INSTRUMENT_H
