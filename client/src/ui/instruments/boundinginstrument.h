#ifndef QN_BOUNDING_INSTRUMENT_H
#define QN_BOUNDING_INSTRUMENT_H

#include <QRect>
#include <QTransform>
#include "instrument.h"
#include "animationtimer.h"

class BoundingInstrument: public Instrument, protected AnimationTimerListener {
    Q_OBJECT;
public:
    BoundingInstrument(QObject *parent = NULL);

    virtual ~BoundingInstrument();

    /**
     * \param view                      Graphics view to use.
     * \param positionBounds            Rectangular area to which viewport movement is restricted, in scene coordinates.
     * \param extension                 Extension of this area, in viewports.
     */
    void setPositionBounds(QGraphicsView *view, const QRectF &positionBounds, qreal extension);

    /**
     * \param view                      Graphics view to use.
     * \param sizeLowerBound            Lower bound of viewport size, in scene coordinates.
     * \param lowerMode                 Mode for the lower bound.
     * \param sizeUpperBound            Upper bound of viewport size, in scene coordinates.
     * \param upperMode                 Mode for the upper bound.
     */
    void setSizeBounds(QGraphicsView *view, const QSizeF &sizeLowerBound, Qt::AspectRatioMode lowerMode, const QSizeF &sizeUpperBound, Qt::AspectRatioMode upperMode);

    /**
     * \param view                      Graphics view to use.
     * \param multiplier                Viewport movement speed, in viewports per second.
     */
    void setMovementSpeed(QGraphicsView *view, qreal multiplier);

    /**
     * \param view                      Graphics view to use.
     * \param multiplier                Scale speed, factor per second.
     */
    void setScalingSpeed(QGraphicsView *view, qreal multiplier);

    /**
     * \param view                      Graphics view to use.
     * \param positionEnforced          Whether position boundary is enforced with animation.
     */
    void setPositionEnforced(QGraphicsView *view, bool positionEnforced = true);

    /**
     * \param view                      Graphics view to use.
     * \param sizeEnforced              Whether size boundary is enforced with animation.
     */
    void setSizeEnforced(QGraphicsView *view, bool sizeEnforced = true);

public slots:
    void enforcePosition(QGraphicsView *view);
    void dontEnforcePosition(QGraphicsView *view);
    void enforceSize(QGraphicsView *view);
    void dontEnforceSize(QGraphicsView *view);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual void enabledNotify() override;

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
