#include "boundinginstrument.h"
#include <cassert>
#include <cmath> /* For std::log, std::exp, std::abs. */
#include <limits>
#include <QGraphicsView>
#include <QScrollBar>
#include <utility/warnings.h>

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

    qreal calculateScale(QSizeF size, QSizeF bounds) {
        return qMax(size.width() / bounds.width(), size.height() / bounds.height());
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

    bool qFuzzyIsNull(const QPointF &p) {
        return ::qFuzzyIsNull(p.x()) && ::qFuzzyIsNull(p.y());
    }

    bool qFuzzyCompare(const QPointF &l, const QPointF &r) {
        return ::qFuzzyCompare(l.x(), r.x()) && ::qFuzzyCompare(l.y(), r.y());
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

    QPointF toPoint(const QSizeF &size) {
        return QPointF(size.width(), size.height());
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

    ViewData(QGraphicsView *view): 
        m_view(view) 
    {
        assert(view != NULL);

        qreal t = 1.0e3;
        setPositionBounds(QRectF(QPointF(-t, -t), QPointF(t, t)), 0.0);
        setMovementSpeed(1.0);
        setSizeBounds(QSizeF(t, t));
        setScalingSpeed(1.0);
        setPositionEnforced(true);
        setSizeEnforced(true);
        
        updateParameters();
    }

    void correct() {
        assert(m_view != NULL);

        if(qFuzzyCompare(m_view->viewportTransform(), m_sceneToViewport))
            return;

        ensureParameters();

        qreal oldScale = m_scale;
        QPointF oldCenter = m_center;

        updateParameters();
        ensureParameters();

        /* Apply zoom correction. */
        if(m_scale > 1.0 && !qFuzzyCompare(m_scale, oldScale)) {
            qreal logOldScale = std::log(oldScale);
            qreal logScale = std::log(m_scale);
            qreal logFactor = calculateCorrection(logOldScale, logScale, -std::numeric_limits<qreal>::max(), 0.0);

            scaleViewport(std::exp(logFactor));

            /* Calculate relative correction and move viewport. */
            qreal correction = logFactor / (logScale - logOldScale);
            moveViewport((m_center - oldCenter) * correction);

            updateParameters();
            ensureParameters();
        }

        /* Apply center correction. */
        if(!m_centerPositionBounds.contains(m_center) && !qFuzzyCompare(m_center, oldCenter)) {
            QPointF correction = calculateCorrection(oldCenter, m_center, m_centerPositionBounds);

            qDebug("CORRECTION %g %g", correction.x(), correction.y());

            moveViewport(correction);

            updateParameters();
        }
    }

    void enforce(qreal dt) {
        assert(m_view != NULL);

        if(!m_isPositionEnforced && !m_isSizeEnforced)
            return;

        ensureParameters();

        /* Apply zoom correction. */
        if(m_isSizeEnforced && m_scale > 1.0) {
            qreal factor = std::exp(-dt * m_logScaleSpeed * speedMultiplier(std::log(m_scale), std::log(2.0)));
            if(m_scale * factor < 1.0)
                factor = 1.0 / m_scale;

            scaleViewport(factor);
        }

        /* Apply move correction. */
        if(m_isPositionEnforced && !m_centerPositionBounds.contains(m_center)) {
            QPointF speed = m_movementSpeed * toPoint(m_sceneViewportRect.size());
            QPointF direction = -calculateDistance(m_centerPositionBounds, m_center);
            QPointF delta = QPointF(
                dt * speed.x() * speedMultiplier(direction.x(), m_sceneViewportRect.width()),
                dt * speed.y() * speedMultiplier(direction.y(), m_sceneViewportRect.height())
            );

            if(std::abs(delta.x()) > std::abs(direction.x()))
                delta.rx() = direction.x();

            if(std::abs(delta.y()) > std::abs(direction.y()))
                delta.ry() = direction.y();

            moveViewport(delta);
        }

        updateParameters();
    }

    void setPositionBounds(const QRectF &positionBounds, qreal extension) {
        m_positionBounds = positionBounds;
        m_positionBoundsExtension = extension;

        updateSceneRect();
        invalidateParameters();
    }

    void setSizeBounds(const QSizeF &sizeBound) {
        m_sizeBounds = sizeBound;

        updateSceneRect();
        invalidateParameters();
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
        m_centerPositionBounds = truncated(InstrumentUtility::dilated(m_positionBounds, (m_positionBoundsExtension - 0.5) * m_sceneViewportRect.size()));
        m_scale = calculateScale(m_sceneViewportRect.size(), m_sizeBounds);
        m_center = m_sceneViewportRect.center();
        m_parametersValid = true;
    }

    void moveViewport(const QPointF &positionDelta) const {
        assert(m_view != NULL);

        QScrollBar *hBar = m_view->horizontalScrollBar();
        QScrollBar *vBar = m_view->verticalScrollBar();
        QPoint delta = (m_sceneToViewport.map(-positionDelta) - m_sceneToViewport.map(QPointF(0.0, 0.0))).toPoint();
        hBar->setValue(hBar->value() + (m_view->isRightToLeft() ? delta.x() : -delta.x()));
        vBar->setValue(vBar->value() - delta.y());
    }

    void scaleViewport(qreal factor) const {
        assert(m_view != NULL);

        qreal sceneFactor = 1 / factor;

        QGraphicsView::ViewportAnchor anchor = m_view->transformationAnchor();
        bool interactive = m_view->isInteractive();
        m_view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
        m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        m_view->scale(sceneFactor, sceneFactor);
        m_view->setTransformationAnchor(anchor);
        m_view->setInteractive(interactive);
    }

    void updateSceneRect() {
        QRectF sceneRect = m_positionBounds;

        /* Adjust to fix size bounds. */
        qreal dx = 0.0, dy = 0.0;
        if(sceneRect.width() < m_sizeBounds.width())
            dx = m_sizeBounds.width() / 2.0;
        if(sceneRect.height() < m_sizeBounds.height())
            dy = m_sizeBounds.height() / 2.0;
        sceneRect.adjust(-dx, -dy, dx, dy);
            
        /* Expand. */
        sceneRect = InstrumentUtility::dilated(sceneRect, sceneRect.size() * 3);

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

    /** Viewport size boundary, in scene coordinates. */
    QSizeF m_sizeBounds;

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

    /** Whether stored parameter values are valid. */
    mutable bool m_parametersValid;

    /** Viewport rectangle, in scene coordinates. */
    mutable QRectF m_sceneViewportRect;

    /** Effective boundary for viewport center. */
    mutable QRectF m_centerPositionBounds;

    /* Viewport scale, relative to size boundary. */
    mutable qreal m_scale;

    /* Position of viewport center. */
    mutable QPointF m_center;
};


BoundingInstrument::BoundingInstrument(QObject *parent):
    Instrument(makeSet(), makeSet(), makeSet(QEvent::Paint), false, parent),
    m_timer(new AnimationTimer(this)),
    m_lastTickTime(0)
{
    m_timer->setListener(this);
    m_timer->start();
}

BoundingInstrument::~BoundingInstrument() {
    foreach(ViewData *d, m_data)
        delete d;
    m_data.clear();
}

void BoundingInstrument::tick(int currentTime) {
    qreal dt = (currentTime - m_lastTickTime) / 1000.0;
    
    foreach(ViewData *d, m_data) {
        d->correct();
        d->enforce(dt);
    }

    m_lastTickTime = currentTime;
}

bool BoundingInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    ViewData *d = cdata(this->view(viewport));
    if(d == NULL)
        return false;

    d->correct();

    return false;
}

BoundingInstrument::ViewData *BoundingInstrument::data(QGraphicsView *view) {
    ViewData *&result = m_data[view];

    if(result == NULL)
        result = new ViewData(view);

    return result;
}

BoundingInstrument::ViewData *BoundingInstrument::cdata(QGraphicsView *view) const {
    return m_data.value(view, NULL);
}

void BoundingInstrument::setPositionBounds(QGraphicsView *view, const QRectF &positionBounds, qreal extension) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setPositionBounds(positionBounds, extension);
}

void BoundingInstrument::setSizeBounds(QGraphicsView *view, const QSizeF &sizeBound) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setSizeBounds(sizeBound);
}

void BoundingInstrument::setMovementSpeed(QGraphicsView *view, qreal multiplier) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setMovementSpeed(multiplier);
}

void BoundingInstrument::setScalingSpeed(QGraphicsView *view, qreal multiplier) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setScalingSpeed(multiplier);
}

void BoundingInstrument::setPositionEnforced(QGraphicsView *view, bool positionEnforced) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setPositionEnforced(positionEnforced);
}

void BoundingInstrument::setSizeEnforced(QGraphicsView *view, bool sizeEnforced) {
    if(view == NULL) {
        qnNullWarning(view);
        return;
    }

    data(view)->setSizeEnforced(sizeEnforced);
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

