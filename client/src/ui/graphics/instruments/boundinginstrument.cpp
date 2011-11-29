#include "boundinginstrument.h"
#include <cassert>
#include <cmath> /* For std::log, std::exp, std::abs. */
#include <limits>
#include <QGraphicsView>
#include <QDateTime>
#include <utils/common/warnings.h>
#include <ui/animation/animation_event.h>
#include "instrumentmanager.h"

namespace {

    qreal calculateCorrection(qreal oldValue, qreal newValue, qreal lowerBound, qreal upperBound) {
        if(oldValue >= lowerBound && oldValue <= upperBound)
            return 0.0;

        return (oldValue - newValue) * 0.5;
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

    QPointF calculateDistance(const QRectF &from, const QPointF &to) {
        QPointF result(0.0, 0.0);

        if(to.x() < from.left()) {
            result.rx() = to.x() - from.left();
        } else if(to.x() > from.right()) {
            result.rx() = to.x() - from.right();
        }

        if(to.y() < from.top()) {
            result.ry() = to.y() - from.top();
        } else if(to.y() > from.bottom()) {
            result.ry() = to.y() - from.bottom();
        }

        return result;
    }

    qreal speedMultiplier(qreal t, qreal oneAt) {
        /* Note that speedMultiplier(oneAt) == 1.0 */

        if(t > 0) {
            return (t / oneAt) * 0.9 + 0.1;
        } else {
            return (t / oneAt) * 0.9 - 0.1;
        }
    }

} // anonymous namespace


class BoundingInstrument::ViewData {
public:
    ViewData(): m_view(NULL) {}

    ViewData(QGraphicsView *view): m_view(NULL) {
        qreal t = 1.0e3;
        setPositionBounds(QRectF(QPointF(-t, -t), QPointF(t, t)), 0.0);
        setMovementSpeed(1.0);
        setSizeBounds(QSizeF(1 / t, 1 / t), Qt::KeepAspectRatioByExpanding, QSizeF(t, t), Qt::KeepAspectRatio);
        setScalingSpeed(1.0);
        setPositionEnforced(false);
        setSizeEnforced(false);
        
        setView(view);
    }

    void correct() {
        assert(m_view != NULL);

        if(qFuzzyCompare(m_view->viewportTransform(), m_sceneToViewport))
            return;

        ensureParameters();

        qreal oldUpperScale = m_upperScale;
        qreal oldLowerScale = m_lowerScale;
        QPointF oldCenter = m_center;

        updateParameters();
        ensureParameters();

        /* Apply zoom correction. */
        if((m_upperScale > 1.0 || m_lowerScale < 1.0) && !qFuzzyCompare(m_upperScale, oldUpperScale)) {
            qreal logOldScale = 0.0, logScale = 0.0, logFactor = 0.0;

            if(m_upperScale > 1.0) {
                logOldScale = std::log(oldUpperScale);
                logScale = std::log(m_upperScale);
                logFactor = calculateCorrection(logOldScale, logScale, -std::numeric_limits<qreal>::max(), 0.0);
            } else {
                logOldScale = std::log(oldLowerScale);
                logScale = std::log(m_lowerScale);
                logFactor = calculateCorrection(logOldScale, logScale, 0.0, std::numeric_limits<qreal>::max());
            }

            QnSceneUtility::scaleViewport(m_view, std::exp(logFactor));

            /* Calculate relative correction and move viewport. */
            qreal correction = logFactor / (logScale - logOldScale);
            QnSceneUtility::moveViewport(m_view, (m_center - oldCenter) * correction);

            updateParameters();
            ensureParameters();
        }

        /* Apply center correction. */
        if(!m_centerPositionBounds.contains(m_center) && !qFuzzyCompare(m_center, oldCenter)) {
            QPointF correction = calculateCorrection(oldCenter, m_center, m_centerPositionBounds);

            QnSceneUtility::moveViewport(m_view, correction);

            updateParameters();
        }
    }

    bool enforceNeeded() const {
        assert(m_view != NULL);

        if(!m_isPositionEnforced && !m_isSizeEnforced)
            return false;

        ensureParameters();

        if(m_isSizeEnforced && (m_upperScale > 1.0 || m_lowerScale < 1.0))
            return true;

        if(m_isPositionEnforced && !m_centerPositionBounds.contains(m_center)) {
            QPointF direction = -calculateDistance(m_centerPositionBounds, m_center);
            if(!qFuzzyIsNull(direction))
                return true;
        }

        return false;
    }

    void enforce(qreal dt) {
        assert(m_view != NULL);

        if(!m_isPositionEnforced && !m_isSizeEnforced)
            return;

        if(qFuzzyIsNull(dt))
            return;

        ensureParameters();

        /* Apply zoom correction. */
        if(m_isSizeEnforced && (m_upperScale > 1.0 || m_lowerScale < 1.0)) {
            qreal factor = 1.0;

            if(m_upperScale > 1.0) {
                factor = std::exp(-dt * m_logScaleSpeed * speedMultiplier(std::log(m_upperScale), std::log(2.0)));
                if(m_upperScale * factor < 1.0)
                    factor = 1.0 / m_upperScale;
            } else {
                factor = std::exp(-dt * m_logScaleSpeed * speedMultiplier(std::log(m_lowerScale), std::log(2.0)));
                if(m_lowerScale * factor > 1.0)
                    factor = 1.0 / m_lowerScale;
            }

            QnSceneUtility::scaleViewport(m_view, factor);
        }

        /* Apply move correction. */
        if(m_isPositionEnforced && !m_centerPositionBounds.contains(m_center)) {
            QPointF direction = -calculateDistance(m_centerPositionBounds, m_center);
            if(!qFuzzyIsNull(direction)) {
                QPointF speed = m_movementSpeed * QnSceneUtility::toPoint(m_sceneViewportRect.size());
                QPointF delta = QPointF(
                    dt * speed.x() * speedMultiplier(direction.x(), m_sceneViewportRect.width()),
                    dt * speed.y() * speedMultiplier(direction.y(), m_sceneViewportRect.height())
                );

                if(std::abs(delta.x()) > std::abs(direction.x()))
                    delta.rx() = direction.x();

                if(std::abs(delta.y()) > std::abs(direction.y()))
                    delta.ry() = direction.y();

                //qDebug() << "ENFORCE" << QDateTime::currentMSecsSinceEpoch() << delta;

                QnSceneUtility::moveViewport(m_view, delta);
            }
        }

        updateParameters();
    }

    void update() {
        updateParameters();
        updateSceneRect();
    }

    QGraphicsView *view() const {
        return m_view;
    }

    void setPositionBounds(const QRectF &positionBounds, qreal extension) {
        m_positionBounds = positionBounds;
        m_positionBoundsExtension = extension;

        if(m_view != NULL) {
            updateSceneRect();
            invalidateParameters();
        }
    }

    void setSizeBounds(const QSizeF &sizeLowerBound, Qt::AspectRatioMode lowerMode, const QSizeF &sizeUpperBound, Qt::AspectRatioMode upperMode) {
        m_sizeLowerBounds = sizeLowerBound;
        m_lowerMode = lowerMode;
        m_sizeUpperBounds = sizeUpperBound;
        m_upperMode = upperMode;

        if(m_view != NULL) {
            updateSceneRect();
            invalidateParameters();
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
            updateParameters();
            updateSceneRect();
        }
    }

    qint64 lastTickTime() const {
        return m_lastTickTime;
    }

    void setLastTickTime(qint64 lastTickTime) {
        m_lastTickTime = lastTickTime;
    }

protected:
    void updateParameters() {
        assert(m_view != NULL);

        m_sceneToViewport = m_view->viewportTransform();
        m_viewportRect = m_view->viewport()->rect();
        m_parametersValid = false;
    }

    void invalidateParameters() {
        assert(m_view != NULL);

        m_parametersValid = false;
    }

    void ensureParameters() const {
        assert(m_view != NULL);

        if(m_parametersValid)
            return;

        m_sceneViewportRect = m_sceneToViewport.inverted().mapRect(QRectF(m_viewportRect));
        m_centerPositionBounds = truncated(QnSceneUtility::dilated(m_positionBounds, (m_positionBoundsExtension - 0.5) * m_sceneViewportRect.size()));
        m_upperScale = 1.0 / scaleFactor(m_sceneViewportRect.size(), m_sizeUpperBounds, m_upperMode);
        m_lowerScale = 1.0 / scaleFactor(m_sceneViewportRect.size(), m_sizeLowerBounds, m_lowerMode);
        m_center = m_sceneViewportRect.center();
        m_parametersValid = true;
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
        sceneRect = QnSceneUtility::dilated(sceneRect, sceneRect.size() * 3);

        /* Expand to include current scene rect. */
        sceneRect = sceneRect.united(m_view->sceneRect());

        m_view->setSceneRect(sceneRect);
    }

public:
    /** Graphics view. */
    QGraphicsView *m_view;

    /** Viewport position boundary, in scene coordinates. */
    QRectF m_positionBounds;

    /** Viewport position boundary extension multiplier. 
     * Viewport size is multiplied  by this factor and added to the sides of position boundary. */
    qreal m_positionBoundsExtension;

    /** Viewport lower size boundary, in scene coordinates. */
    QSizeF m_sizeLowerBounds;

    /** Scale mode for the lower size boundary. */
    Qt::AspectRatioMode m_lowerMode;

    /** Viewport upper size boundary, in scene coordinates. */
    QSizeF m_sizeUpperBounds;

    /** Scale mode for the upper size boundary. */
    Qt::AspectRatioMode m_upperMode;

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

    /** Scene-to-viewport transformation. */
    QTransform m_sceneToViewport;

    /** Viewport rectangle, in viewport coordinates. */
    QRect m_viewportRect;

    /** Last time the timer ticked. */
    qint64 m_lastTickTime;

    /** Whether stored parameter values are valid. */
    mutable bool m_parametersValid;

    /** Viewport rectangle, in scene coordinates. */
    mutable QRectF m_sceneViewportRect;

    /** Effective boundary for viewport center. */
    mutable QRectF m_centerPositionBounds;

    /* Viewport scale, relative to upper size boundary. */
    mutable qreal m_upperScale;

    /* Viewport scale, relative to lower size boundary. */    
    mutable qreal m_lowerScale;

    /* Position of viewport center. */
    mutable QPointF m_center;
};


BoundingInstrument::BoundingInstrument(QObject *parent):
    Instrument(VIEWPORT, makeSet(QEvent::Paint, AnimationEvent::Animation), parent)
{
    animationTimer()->addListener(this);

    m_data[NULL] = new ViewData(NULL);
}

BoundingInstrument::~BoundingInstrument() {
    ensureUninstalled();

    foreach(ViewData *data, m_data)
        delete data;
}

void BoundingInstrument::enabledNotify() {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    foreach(ViewData *d, m_data) {
        if(d->view() != NULL) {
            d->update();
            d->setLastTickTime(currentTime);
        }
    }

    animationTimer()->activate();
}

void BoundingInstrument::aboutToBeDisabledNotify() {
    animationTimer()->deactivate();    
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

void BoundingInstrument::tick(int /*deltaTime*/) {
    foreach(ViewData *d, m_data) {
        if(d->view() == NULL)
            continue;

        if(d->enforceNeeded())
            d->view()->update();
    }
}

bool BoundingInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    QGraphicsView *view = this->view(viewport);

    ViewData *d = m_data.value(view, NULL);
    if(d == NULL)
        return false;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qreal dt = (currentTime - d->lastTickTime()) / 1000.0;
    d->setLastTickTime(currentTime);

    d->correct();
    d->enforce(dt);

    return false;
}

BoundingInstrument::ViewData *BoundingInstrument::checkView(QGraphicsView *view) const {
    ViewData *d = m_data.value(view, NULL);
    if(d == NULL)
        qnWarning("Given graphics view is not registered with this bounding instrument.");
    
    return d;
}

void BoundingInstrument::setPositionBounds(QGraphicsView *view, const QRectF &positionBounds, qreal extension) {
    if(view == NULL) {
        foreach(ViewData *d, m_data)
            d->setPositionBounds(positionBounds, extension);
    } else if(ViewData *d = checkView(view)) {
        d->setPositionBounds(positionBounds, extension);
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

