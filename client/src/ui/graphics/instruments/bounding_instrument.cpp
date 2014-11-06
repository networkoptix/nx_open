#include "bounding_instrument.h"

#include <cassert>
#include <cmath> /* For std::log, std::exp, std::abs. */
#include <limits>

#include <QtWidgets/QGraphicsView>
#include <QtCore/QDateTime>
#include <QtGui/QMatrix4x4>

#include <utils/math/fuzzy.h>
#include <utils/common/warnings.h>
#include <ui/animation/animation_event.h>

namespace {

    qreal calculateCorrection(qreal oldValue, qreal newValue, qreal lowerBound, qreal upperBound) {
        if(oldValue > newValue)
            return -calculateCorrection(newValue, oldValue, lowerBound, upperBound);
        /* Here oldValue <= newValue. */

        qreal result = 0.0;

        /* Adjust for boundary intersections. */
        if(oldValue < lowerBound && lowerBound < newValue) {
            result += -(lowerBound - oldValue) * 0.5;
            oldValue = lowerBound;
        }
        if(oldValue < upperBound && upperBound < newValue) {
            result += -(newValue - upperBound) * 0.5;
            newValue = upperBound;
        }

        /* At this point, both old & new values lie in the same section. */
        if(oldValue < lowerBound || newValue > upperBound)
            result += -(newValue - oldValue) * 0.5;

        return result;
    }

    QPointF calculateCorrection(QPointF oldValue, QPointF newValue, QRectF bounds) {
        return QPointF(
            calculateCorrection(oldValue.x(), newValue.x(), bounds.left(), bounds.right()),
            calculateCorrection(oldValue.y(), newValue.y(), bounds.top(),  bounds.bottom())
        );
    }

    QRectF truncated(const QRectF &rect) {
        QRectF result = rect;

        if(result.width() < 0) {
            result.moveLeft(result.left() + result.width() / 2);
            result.setWidth(0);
        }

        if(result.height() < 0) {
            result.moveTop(result.top() + result.height() / 2);
            result.setHeight(0);
        }

        return result;
    }

    qreal calculateDistance(qreal from, qreal lo, qreal hi) {
        if(from < lo) {
            return lo - from;
        } else if(from > hi) {
            return hi - from;
        } else {
            return 0.0;
        }
    }

    QPointF calculateDistance(const QPointF &from, const QRectF &to) {
        return QPointF(
            calculateDistance(from.x(), to.left(), to.right()),
            calculateDistance(from.y(), to.top(), to.bottom())
        );
    }

    qreal speedMultiplier(qreal t, qreal oneAt) {
        /* Note that speedMultiplier(oneAt) == 1.0 and speedMultiplier(-oneAt) == -1.0 */

        if(t > 0) {
            return (t / oneAt) * 0.9 + 0.1;
        } else {
            return (t / oneAt) * 0.9 - 0.1;
        }
    }

    /**
     * Calculates fixed point of an affine transformation. Perspective 
     * transformations are not handled.
     * 
     * \param t                         Transformation to calculate fixed point of.
     * \param[out] exists               Whether the fixed point exists.
     * \returns                         Fixed point, or (0, 0) if it doesn't exist.
     */
    QPointF calculateFixedPoint(const QTransform &t, bool *exists) {
        /* Check that it actually is a non-perspective transform. */
#ifdef _DEBUG
        if(!qFuzzyIsNull(t.m13()) || !qFuzzyIsNull(t.m23()) || !qFuzzyCompare(t.m33(), 1.0))
            qnWarning("Perspective transformation supplied, expect invalid results.");
#endif

        /* Fill in row-major order. */
        QMatrix4x4 m(
            t.m11() - 1.0,  t.m21(),       0.0,   0.0,
            t.m12(),        t.m22() - 1.0, 0.0,   0.0,
            0.0,            0.0,           1.0,   0.0,
            0.0,            0.0,           0.0,   1.0
        );

        /* We deal with stability issues the brute way. */
        if(qFuzzyIsNull(m(0, 0))) m(0, 0) = 0.0;
        if(qFuzzyIsNull(m(1, 0))) m(1, 0) = 0.0;
        if(qFuzzyIsNull(m(1, 1))) m(1, 1) = 0.0;
        if(qFuzzyIsNull(m(0, 1))) m(0, 1) = 0.0;

        /* Solve linear system. */
        bool invertible;
        QVector4D v = m.inverted(&invertible) * QVector4D(
            -t.m31(),
            -t.m32(),
            0.0,
            0.0
        );

        if(exists != NULL)
            *exists = invertible;

        if(invertible) {
            return QPointF(v.x(), v.y());
        } else {
            return QPointF();
        }
    }

} // anonymous namespace


class BoundingInstrument::ViewData: protected QnSceneTransformations {
public:
    ViewData(): 
        m_view(NULL) 
    {
        qreal d = 1.0e3;
        setPositionBounds(QRectF(QPointF(-d, -d), QPointF(d, d)));
        setPositionBoundsExtension(MarginsF(0.0, 0.0, 0.0, 0.0));
        setMovementSpeed(1.0);
        setSizeBounds(QSizeF(1 / d, 1 / d), Qt::KeepAspectRatioByExpanding, QSizeF(d, d), Qt::KeepAspectRatio);
        setSizeBoundsExtension(QSizeF(0.0, 0.0), QSizeF(0.0, 0.0));
        setScalingSpeed(1.0);
        setPositionEnforced(false);
        setSizeEnforced(false);
        setView(NULL);

        m_isSizeCorrected = false;
        m_isPositionCorrected = true;

        m_lastTickTime = 0;
        m_stickyLogScaleHi = 1.0;
        m_stickyLogScaleLo = -1.0;
        m_defaultStickyLogScaleResettingThreshold = 0.7;
        m_stickyLogScaleResettingThreshold = m_defaultStickyLogScaleResettingThreshold;
        m_logScaleResettingSpeedMultiplier = 0.16;
    }

    void update() {
        updateParameters();
        updateSceneRect();
    }

    QGraphicsView *view() const {
        return m_view;
    }

    void setPositionBounds(const QRectF &positionBounds) {
        m_positionBounds = positionBounds;

        if(m_view != NULL)
            updateSceneRect();
    }

    const QRectF &positionBounds() const {
        return m_positionBounds;
    }

    void setPositionBoundsExtension(const MarginsF &extension) {
        m_positionBoundsExtension = extension;

        if (m_view)
            updateSceneRect();
    }

    const MarginsF &positionBoundsExtension() const {
        return m_positionBoundsExtension;
    }

    void setSizeBounds(const QSizeF &sizeLowerBound, Qt::AspectRatioMode lowerMode, const QSizeF &sizeUpperBound, Qt::AspectRatioMode upperMode) {
        m_sizeLowerBounds = sizeLowerBound;
        m_lowerMode = lowerMode;
        m_sizeUpperBounds = sizeUpperBound;
        m_upperMode = upperMode;

        if(!contains(m_sizeUpperBounds, m_sizeLowerBounds))
            m_sizeUpperBounds = m_sizeLowerBounds;

        if(m_view != NULL) {
            updateExtendedSizeBounds();
            updateSceneRect();
        }
    }

    void setSizeBoundsExtension(const QSizeF &sizeLowerExtension, const QSizeF &sizeUpperExtension) {
        m_sizeLowerExtension = sizeLowerExtension;
        m_sizeUpperExtension = sizeUpperExtension;

        if (m_view) {
            updateExtendedSizeBounds();
            updateSceneRect();
        }
    }

    void setMovementSpeed(qreal multiplier) {
        m_movementSpeed = multiplier;
    }

    void setScalingSpeed(qreal multiplier) {
        m_logScaleSpeed = std::log(multiplier);
    }

    void setPositionEnforced(bool positionEnforced) {
        m_isPositionEnforced = positionEnforced;
    }

    void setSizeEnforced(bool sizeEnforced) {
        m_isSizeEnforced = sizeEnforced;
    }

    void setView(QGraphicsView *view) {
        m_view = view;

        if(m_view != NULL) {
            updateSceneRect();
            updateParameters();
        }
    }

    void setLastTickTime(qint64 lastTickTime) {
        m_lastTickTime = lastTickTime;
    }

    void tick(qint64 time) {
        assert(m_view != NULL);

        qint64 dtMSec = time - m_lastTickTime;
        qreal dt = dtMSec / 1000.0;

        /** Whether sticky scale values need to be adjusted. */
        bool stickyScaleDirty = false;

        /* Scene viewport center at the end of the last tick. */
        QPointF oldCenter = m_sceneViewportCenter;
        
        /* Scene to viewport transform at the end of the last tick. */
        //QTransform oldSceneToViewport = m_sceneToViewport;

        /* Correct. */

        /* Calculate old scale. */
        qreal logOldScale = 1.0;
        if(m_isSizeCorrected)
            calculateRelativeScale(&logOldScale);

        /* Calculate fixed point. */
        bool fixedPointExists;
        QPoint fixedPoint = calculateFixedPoint(m_viewportToScene * m_view->viewportTransform(), &fixedPointExists).toPoint();
        if(fixedPointExists && m_viewportRect.contains(fixedPoint))
            m_fixedPoint = fixedPoint;

        updateParameters();

        if (!qFuzzyCompare(m_view->viewportTransform(), m_sceneToViewport)) {
            /* Apply zoom correction. */
            if(m_isSizeCorrected) {
                qreal logScale, powFactor;
                calculateRelativeScale(&logScale, &powFactor);
                if(!qFuzzyBetween(m_stickyLogScaleLo, logScale, m_stickyLogScaleHi) && !qFuzzyCompare(logScale, logOldScale)) {
                    qreal logFactor = calculateCorrection(logOldScale, logScale, m_stickyLogScaleLo, m_stickyLogScaleHi);

                    scaleViewport(m_view, std::exp(logFactor * powFactor));

                    /* Calculate relative correction and move viewport. */
                    qreal correction = logFactor / (logOldScale - logScale); /* Always positive. */
                    moveViewportScene(m_view, (oldCenter - m_sceneViewportCenter) * correction);

                    updateParameters();
                } else {
                    stickyScaleDirty = true;
                }
            }

            /* Apply center correction. */
            QRectF centerPositionBounds = calculateCenterPositionBounds();
            if(!centerPositionBounds.contains(m_sceneViewportCenter) && !qFuzzyEquals(m_sceneViewportCenter, oldCenter)) {
                QPointF correction = calculateCorrection(oldCenter, m_sceneViewportCenter, centerPositionBounds);

                moveViewportScene(m_view, correction);

                updateParameters();
            }
        }

        /* Enforce. */
        if(dtMSec != 0) {
            /* Apply zoom correction. */
            if(m_isSizeEnforced) {
                qreal logScale, powFactor;
                calculateRelativeScale(&logScale, &powFactor);

                qreal logDirection = calculateDistance(logScale, m_stickyLogScaleLo, m_stickyLogScaleHi);
                qreal scaleSpeed = m_logScaleSpeed;
                if (qFuzzyIsNull(logDirection)) {
                    if (logScale > m_stickyLogScaleResettingThreshold) {
                        logDirection = 1.0 - logScale;
                        scaleSpeed *= m_logScaleResettingSpeedMultiplier;
                    }
                }
                if(!qFuzzyIsNull(logDirection)) {
                    qreal logDelta = dt * (scaleSpeed / powFactor) * speedMultiplier(logDirection, std::log(2.0) / powFactor);
                    if(std::abs(logDelta) > std::abs(logDirection))
                        logDelta = logDirection;

                    scaleViewport(m_view, std::exp(logDelta * powFactor), m_fixedPoint);

                    updateParameters();
                } else {
                    stickyScaleDirty = true;
                }
            }

            /* Apply move correction. */
            if(m_isPositionEnforced) {
                QRectF centerPositionBounds = calculateCenterPositionBounds();
                if(!centerPositionBounds.contains(m_sceneViewportCenter)) {
                    QPointF direction = calculateDistance(m_sceneViewportCenter, centerPositionBounds);
                    if(!qFuzzyIsNull(direction)) {
                        qreal viewportSize = (m_sceneViewportRect.width() + m_sceneViewportRect.height()) / 2;
                        qreal speed = m_movementSpeed * viewportSize;
                        qreal directionLength = length(direction);
                        qreal deltaLength = dt * speed * speedMultiplier(directionLength, viewportSize);
                        if(deltaLength > directionLength)
                            deltaLength = directionLength;

                        moveViewportScene(m_view, direction / directionLength * deltaLength);
                    }
                }
            }

            updateParameters();
        }

        /* Adjust sticky scale if needed. */
        if (stickyScaleDirty && (!qFuzzyCompare(m_stickyLogScaleHi, 1.0) || !qFuzzyCompare(m_stickyLogScaleLo, 1.0))) {
            qreal logScale, powFactor;
            calculateRelativeScale(&logScale, &powFactor);

            m_stickyLogScaleLo = qMin(-1.0, qMax(logScale, m_stickyLogScaleLo));
            m_stickyLogScaleHi = qMax( 1.0, qMin(logScale, m_stickyLogScaleHi));
            if (!qFuzzyCompare(logScale, m_stickyLogScaleResettingThreshold))
                m_stickyLogScaleResettingThreshold = m_defaultStickyLogScaleResettingThreshold;
        }

        m_lastTickTime = time;
    }

    void stickScale() {
        qreal logScale, powFactor;
        calculateRelativeScale(&logScale, &powFactor);

        m_stickyLogScaleLo = qMin(-1.0, logScale);
        m_stickyLogScaleHi = qMax(1.0, logScale);
        m_stickyLogScaleResettingThreshold = logScale;
    }

protected:
    void updateParameters() {
        assert(m_view != NULL);

        m_sceneToViewport = m_view->viewportTransform();
        m_viewportToScene = m_sceneToViewport.inverted();
        m_viewportRect = m_view->viewport()->rect();
        m_sceneViewportRect = m_viewportToScene.mapRect(QRectF(m_viewportRect));
        m_sceneViewportCenter = m_sceneViewportRect.center();
    }

    QRectF calculateCenterPositionBounds() const {
        return truncated(dilated(m_positionBounds, cwiseMul(m_positionBoundsExtension - MarginsF(0.5, 0.5, 0.5, 0.5), m_sceneViewportRect.size())));
    }

    /**
     * Calculates viewport's relative current scale, presuming lower and upper
     * scale bounds are at -1.0 and 1.0.
     */ 
    void calculateRelativeScale(qreal *logScale, qreal *powFactor = NULL) const {
        qreal logLowerScale = std::log(scaleFactor(m_sceneViewportRect.size(), m_extendedSizeLowerBound, m_lowerMode));
        qreal logUpperScale = std::log(scaleFactor(m_sceneViewportRect.size(), m_extendedSizeUpperBound, m_upperMode));

        qreal localFactor = (logUpperScale - logLowerScale) * 0.5;
        *logScale = (0.0 - logLowerScale) / localFactor - 1.0;
        if(powFactor != NULL)
            *powFactor = localFactor;
    }

    void updateSceneRect() {
        assert(m_view != NULL);

        QRectF sceneRect = m_positionBounds;

        /* Adjust to include size upper bound. */
        qreal dx = 0.0, dy = 0.0;
        if(sceneRect.width() < m_sizeUpperBounds.width())
            dx = (m_sizeUpperBounds.width() - sceneRect.width()) / 2.0;
        if(sceneRect.height() < m_sizeUpperBounds.height())
            dy = (m_sizeUpperBounds.height() - sceneRect.height()) / 2.0;
        sceneRect.adjust(-dx, -dy, dx, dy);
            
        /* Expand. */
        sceneRect = QnGeometry::dilated(sceneRect, sceneRect.size() * 3);

        /* Expand to include current scene rect. */
        sceneRect = sceneRect.united(m_view->sceneRect());

        m_view->setSceneRect(sceneRect);
    }

    void updateExtendedSizeBounds() {
        m_extendedSizeUpperBound = m_sizeUpperBounds + cwiseMul(m_sizeUpperExtension, m_sizeUpperBounds);
        m_extendedSizeLowerBound = m_sizeLowerBounds + cwiseMul(m_sizeLowerExtension, m_sizeLowerBounds);
    }

public:
    /* 'Stable' state. */

    /** Graphics view. */
    QGraphicsView *m_view;

    /** Viewport position boundary, in scene coordinates. */
    QRectF m_positionBounds;

    /** Viewport position boundary extension multipliers. 
     * Viewport size is multiplied by the extension factor and added to the sides of position boundary. */
    MarginsF m_positionBoundsExtension;

    /** Viewport lower size boundary, in scene coordinates. */
    QSizeF m_sizeLowerBounds;

    /** Scale mode for the lower size boundary. */
    Qt::AspectRatioMode m_lowerMode;

    /** Extension of the lower bound of viewport size, relative to this lower bound. */
    QSizeF m_sizeLowerExtension;

    /** Viewport upper size boundary, in scene coordinates. */
    QSizeF m_sizeUpperBounds;

    /** Scale mode for the upper size boundary. */
    Qt::AspectRatioMode m_upperMode;

    /** Extension of the upper bound of viewport size, relative to this upper bound. */
    QSizeF m_sizeUpperExtension;

    /** Movement speed, in viewports. 
     * This is a speed at one viewport away from movement boundary.
     * Effective move speed varies depending on the distance to movement boundary. */
    qreal m_movementSpeed;

    /** Logarithmic viewport scaling speed. 
     * This is a speed at 2x scale away from the size boundary. 
     * Effective scaling speed varies depending on current viewport size. */
    qreal m_logScaleSpeed;

    /** Whether size boundary is enforced with animation. */
    bool m_isSizeEnforced;

    /** Whether position boundary is enforced with animation. */
    bool m_isPositionEnforced;

    /** Whether zooming outsize size boundary is corrected. */
    bool m_isSizeCorrected;

    /** Whether motion outside position boundary is corrected. */
    bool m_isPositionCorrected;


    /* 'Derived' state. */

    /** Viewport upper size boundary, with extension applied, in scene coordinates. */
    QSizeF m_extendedSizeUpperBound;

    /** Viewport upper size boundary, with extension applied, in scene coordinates. */
    QSizeF m_extendedSizeLowerBound;


    /* 'Working' state. */

    /** Time of the last tick. */
    qint64 m_lastTickTime;

    /** Coordinates of the fixed point of last transformation, in viewport coordinates. */
    QPoint m_fixedPoint;

    /** Scene-to-viewport transformation. */
    QTransform m_sceneToViewport;

    /** Viewport-to-scene transformation. */
    QTransform m_viewportToScene;

    /** Viewport rectangle, in viewport coordinates. */
    QRect m_viewportRect;

    /** Viewport rectangle, in scene coordinates. */
    QRectF m_sceneViewportRect;

    /** Position of viewport center, in scene coordinates. */
    QPointF m_sceneViewportCenter;

    /** Sticky log scale upper bound. */
    qreal m_stickyLogScaleHi;

    /** Sticky log scale lower bound. */
    qreal m_stickyLogScaleLo;

    /** Threshold multiplier to specify the scale when the bounds should be reset to default value (m_stickyLogScaleHi). */
    qreal m_stickyLogScaleResettingThreshold;

    /** Default value of m_stickyLogScaleResettingThreshold. */
    qreal m_defaultStickyLogScaleResettingThreshold;

    /** Scale speed multiplier when scale is resetting to default value (m_stickyLogScaleHi). */
    qreal m_logScaleResettingSpeedMultiplier;
};


BoundingInstrument::BoundingInstrument(QObject *parent):
    Instrument(Viewport, makeSet(QEvent::Paint, AnimationEvent::Animation), parent)
{
    m_data[NULL] = new ViewData();
}

BoundingInstrument::~BoundingInstrument() {
    ensureUninstalled();

    qDeleteAll(m_data);
}

void BoundingInstrument::enabledNotify() {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    foreach(ViewData *d, m_data) {
        if(d->view() != NULL) {
            d->update();
            d->setLastTickTime(currentTime);
        }
    }
}

bool BoundingInstrument::registeredNotify(QGraphicsView *view) {
    ViewData *&d = m_data[view];

    assert(d == NULL);

    d = new ViewData(*m_data[NULL]);
    d->setView(view);

    return true;
}

void BoundingInstrument::unregisteredNotify(QGraphicsView *view) {
    ViewData *d = m_data.take(view);
    
    assert(d != NULL);

    delete d;
}

bool BoundingInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    QGraphicsView *view = this->view(viewport);

    ViewData *d = m_data.value(view, NULL);
    if(d == NULL)
        return false;

    d->tick(QDateTime::currentMSecsSinceEpoch());

    return false;
}

BoundingInstrument::ViewData *BoundingInstrument::checkView(QGraphicsView *view) const {
    ViewData *d = m_data.value(view, NULL);
    if(d == NULL)
        qnWarning("Given graphics view is not registered with this bounding instrument.");
    
    return d;
}

void BoundingInstrument::setPositionBounds(QGraphicsView *view, const QRectF &positionBounds) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setPositionBounds(positionBounds);
    } else if(ViewData *d = checkView(view)) {
        d->setPositionBounds(positionBounds);
    }
}

QRectF BoundingInstrument::positionBounds(QGraphicsView *view) const {
    if(ViewData *d = checkView(view)) {
        return d->positionBounds();
    } else {
        return QRectF();
    }
}

void BoundingInstrument::setPositionBoundsExtension(QGraphicsView *view, const MarginsF &extension) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setPositionBoundsExtension(extension);
    } else if(ViewData *d = checkView(view)) {
        d->setPositionBoundsExtension(extension);
    }
}

MarginsF BoundingInstrument::positionBoundsExtension(QGraphicsView *view) const {
    if(ViewData *d = checkView(view)) {
        return d->positionBoundsExtension();
    } else {
        return MarginsF();
    }
}

void BoundingInstrument::setSizeBounds(QGraphicsView *view, const QSizeF &sizeLowerBound, Qt::AspectRatioMode lowerMode, const QSizeF &sizeUpperBound, Qt::AspectRatioMode upperMode) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setSizeBounds(sizeLowerBound, lowerMode, sizeUpperBound, upperMode);
    } else if(ViewData *d = checkView(view)) {
        d->setSizeBounds(sizeLowerBound, lowerMode, sizeUpperBound, upperMode);
    }
}

void BoundingInstrument::setSizeBoundsExtension(QGraphicsView *view, const QSizeF &sizeLowerExtension, const QSizeF &sizeUpperExtension) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setSizeBoundsExtension(sizeLowerExtension, sizeUpperExtension);
    } else if(ViewData *d = checkView(view)) {
        d->setSizeBoundsExtension(sizeLowerExtension, sizeUpperExtension);
    }
}

void BoundingInstrument::setMovementSpeed(QGraphicsView *view, qreal multiplier) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setMovementSpeed(multiplier);
    } else if(ViewData *d = checkView(view)) {
        d->setMovementSpeed(multiplier);
    }
}

void BoundingInstrument::setScalingSpeed(QGraphicsView *view, qreal multiplier) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setScalingSpeed(multiplier);
    } else if(ViewData *d = checkView(view)) {
        d->setScalingSpeed(multiplier);
    }
}

void BoundingInstrument::setPositionEnforced(QGraphicsView *view, bool positionEnforced) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setPositionEnforced(positionEnforced);
    } else if(ViewData *d = checkView(view)) {
        d->setPositionEnforced(positionEnforced);
    }
}

void BoundingInstrument::setSizeEnforced(QGraphicsView *view, bool sizeEnforced) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setSizeEnforced(sizeEnforced);
    } else if(ViewData *d = checkView(view)) {
        d->setSizeEnforced(sizeEnforced);
    }
}

void BoundingInstrument::enforcePosition(QGraphicsView *view) {
    setPositionEnforced(view, true);
}

void BoundingInstrument::dontEnforcePosition(QGraphicsView *view) {
    setPositionEnforced(view, false);
}

void BoundingInstrument::enforceSize(QGraphicsView *view) {
    setSizeEnforced(view, true);
}

void BoundingInstrument::dontEnforceSize(QGraphicsView *view) {
    setSizeEnforced(view, false);
}

void BoundingInstrument::stickScale(QGraphicsView *view) {
    if(ViewData *d = checkView(view))
        d->stickScale();
}
