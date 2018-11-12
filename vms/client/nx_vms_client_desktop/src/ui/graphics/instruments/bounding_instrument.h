#ifndef QN_BOUNDING_INSTRUMENT_H
#define QN_BOUNDING_INSTRUMENT_H

#include <QtCore/QRect>
#include <QtCore/QSet>
#include <QtGui/QTransform>
#include <ui/animation/animation_timer.h>
#include "instrument.h"

class BoundingInstrument: public Instrument
{
    Q_OBJECT

public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object.
     */
    BoundingInstrument(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~BoundingInstrument();

    /**
     * \param view                      Graphics view to use.
     * \param positionBounds            Rectangular area to which viewport movement is restricted, in scene coordinates.
     */
    void setPositionBounds(QGraphicsView *view, const QRectF &positionBounds);

    /**
     * \param view                      Graphics view to use.
     * \returns                         Rectangular area to which viewport movement is restricted, in scene coordinates.
     */
    QRectF positionBounds(QGraphicsView *view) const;

    /**
     * \param view                      Graphics view to use.
     * \param extension                 Extension of the area to which viewport movement is restricted, in viewports.
     */
    void setPositionBoundsExtension(QGraphicsView *view, const QMarginsF &extension);

    /**
     * \param view                      Graphics view to use.
     * \returns                         Extension of the area to which viewport movement is restricted, in viewports.
     */
    QMarginsF positionBoundsExtension(QGraphicsView *view) const;

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
     * \param sizeLowerExtension        Extension of the lower bound of viewport size, relative to this lower bound.
     * \param sizeLowerExtension        Extension of the upper bound of viewport size, relative to this upper bound.
     */
    void setSizeBoundsExtension(QGraphicsView *view, const QSizeF &sizeLowerExtension, const QSizeF &sizeUpperExtension);

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

    /**
     * Enforces the given graphics view to preserve its current scale, even if 
     * it lies outside the scale boundaries. Thus, the scale boundaries are
     * effectively extended to include the current scale. However, they 
     * will automatically be contracted once the graphics view's scale changes.
     *
     * \param view                      Graphics view to use.
     */
    void stickScale(QGraphicsView *view);

public slots:
    void enforcePosition(QGraphicsView *view);
    void dontEnforcePosition(QGraphicsView *view);
    void enforceSize(QGraphicsView *view);
    void dontEnforceSize(QGraphicsView *view);

protected:
    virtual void enabledNotify() override;

    virtual bool registeredNotify(QGraphicsView *view) override;
    virtual void unregisteredNotify(QGraphicsView *view) override;

    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    class ViewData;

    ViewData *checkView(QGraphicsView *view) const;

private:
    /** View to data mapping. */
    QHash<QGraphicsView *, ViewData *> m_data;
};

#endif // QN_BOUNDING_INSTRUMENT_H
