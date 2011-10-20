#ifndef QN_BOUNDING_INSTRUMENT_H
#define QN_BOUNDING_INSTRUMENT_H

#include "instrument.h"
#include <QRect>
#include <QTransform>

class BoundingInstrument: public Instrument {
    Q_OBJECT;
public:
    BoundingInstrument(QObject *parent = NULL);

    void setView(QGraphicsView *view);

    /**
     * \param moveBounds                Rectangular area to which unobstructed
     *                                  moving is restricted, in scene coordinates.
     * \param extension                 Extension of this area, in viewports.
     */
    void setMoveBounds(const QRectF &moveBounds, qreal extension);

    /**
     * \param zoomBound                 Maximal size of the viewport to which
     *                                  unobstructed zooming is restricted,
     *                                  in scene coordinates.
     */
    void setZoomBounds(const QSizeF &zoomBound);

    /**
     * \param speed                     Move speed, in scene coordinate units per second.
     * \param extension                 Move speed extension, in viewports per second.
     */
    void setMoveSpeed(qreal speed, qreal extension);

    /**
     * \param multiplier                Zoom speed, factor per second.
     */
    void setZoomSpeed(qreal multiplier);

    /**
     * \param moveAnimated              Whether obstructed moving is animated.
     */
    void setMoveAnimated(bool moveAnimated);

    /**
     * \param zoomAnimated              Whether obstructed zooming is animated.
     */
    void setZoomAnimated(bool zoomAnimated);

protected:
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

private:
    QGraphicsView *m_view;

    QRectF m_moveBounds;
    qreal m_moveBoundsExtension;
    qreal m_moveSpeed;
    qreal m_moveSpeedExtension;
    
    QSizeF m_zoomBounds;
    qreal m_zoomSpeed;

    bool m_isZoomAnimated;
    bool m_isMoveAnimated;

    /** Old viewport-to-scene transformation. */
    QTransform m_oldViewportToScene;

    /* Old zoom, relative to zoom bounds. */
    qreal m_oldZoom;

    /* Old viewport center. */
    QPointF m_oldCenter;
};

#endif // QN_BOUNDING_INSTRUMENT_H
