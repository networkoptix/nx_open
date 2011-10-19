#include "boundinginstrument.h"
#include <QGraphicsView>

namespace {
    qreal calculateCorrectionValue(qreal border, qreal normal, qreal newValue, qreal oldValue) {
        if(border > normal)
            return -calculateCorrectionValue(-border, -normal, -newValue, -oldValue);

        /* Now we can safely assume that border < normal. */

        if(oldValue > normal) {
            if(newValue > normal) {
                return 0.0; /* No correction. */
            } else {
                oldValue = normal;
            }
        }

        if(oldValue < border)
            return oldValue - newValue; /* No movement outside the borders. */
        
        /* Now oldValue lies in [border, normal]. Map it into [0, 1] segment, 0 at normal, 1 at border. */
        qreal alpha = (normal - oldValue) / (normal - border);

        qreal correction = (oldValue - newValue) * alpha;
        if(newValue + correction < border)
            correction = (oldValue + border) / 2 - newValue;

        return correction;        
    }

    void calculateExpansionTranslation(qreal p1, qreal p2, qreal *expansion, qreal *translation) {
        if(p1 > 0) {
            if(p2 > 0) {
                *expansion = 0.0;
                *translation = qMax(p1, p2);
            } else {
                *expansion = qMax(-p1, p2);
                *translation = p1 + p2;
            }
        } else {
            if(p2 < 0) {
                *expansion = 0.0;
                *translation = qMin(p1, p2);
            } else {
                *expansion = qMin(-p1, p2);
                *translation = p1 + p2;
            }
        }
    }

} // anonymous namespace


BoundingInstrument::BoundingInstrument(QObject *parent):
    Instrument(makeSet(), makeSet(QEvent::Resize), makeSet(QEvent::Paint), false, parent),
    m_view(NULL),
    m_isZoomAnimated(true),
    m_isMoveAnimated(true)
{}

void BoundingInstrument::setView(QGraphicsView *view) {

}


bool BoundingInstrument::resizeEvent(QGraphicsView *view, QResizeEvent *event) {
    return false;
}

bool BoundingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(m_view == NULL)
        return false;

    if(m_view->viewport() != viewport)
        return false;

    if(m_view->viewportTransform() == m_viewToScene)
        return false;

    m_oldViewToScene = m_viewToScene;
    m_oldViewportRect = m_viewportRect;

    m_viewToScene = m_view->viewportTransform();
    m_viewportRect = mapRectToScene(m_view, viewport->rect());
    
    QRectF moveRect = dilated(m_moveRect, m_moveRectExtension * m_viewportRect.size());
    QRectF moveBorder = dilated(moveRect, m_moveBorderExtension * m_viewportRect.size() + QSizeF(m_moveBorder, m_moveBorder));

    qreal xp1, yp1, xp2, yp2;
    xp1 = calculateCorrectionValue(moveBorder.left(),   moveRect.left(),   m_viewportRect.left(),   m_oldViewportRect.left());
    xp2 = calculateCorrectionValue(moveBorder.right(),  moveRect.right(),  m_viewportRect.right(),  m_oldViewportRect.right());
    yp1 = calculateCorrectionValue(moveBorder.top(),    moveRect.top(),    m_viewportRect.top(),    m_oldViewportRect.top());
    yp2 = calculateCorrectionValue(moveBorder.bottom(), moveRect.bottom(), m_viewportRect.bottom(), m_oldViewportRect.bottom());

    qreal xShrink, yShrink;
    shrinkAdjust(&xp1, &xp2, &xShrink);
    
    
    return false;
}

void BoundingInstrument::setMoveRect(const QRectF &moveRect, qreal extensionMultiplier) {
        
}

void BoundingInstrument::setZoomRect(const QRectF &zoomRect, qreal extensionMultiplier) {
    
}

void BoundingInstrument::setMoveBorder(qreal moveBorder, qreal extensionMultiplier) {
    
}

void BoundingInstrument::setZoomBorder(qreal zoomBorder, qreal extensionMultiplier) {
    
}

void BoundingInstrument::setSpeed(qreal speed, qreal extensionMultiplier) {
    
}

void BoundingInstrument::setZoomAnimated(bool animated) {
    
}
