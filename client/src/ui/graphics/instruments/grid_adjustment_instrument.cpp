#include "grid_adjustment_instrument.h"
#include <cassert>
#include <QGraphicsSceneWheelEvent>
#include <QWheelEvent>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_grid_mapper.h>

namespace {

}

GridAdjustmentInstrument::GridAdjustmentInstrument(QnWorkbench *workbench, QObject *parent):
    Instrument(
        makeSet(QEvent::Wheel), 
        makeSet(),
        makeSet(QEvent::GraphicsSceneWheel),
        makeSet(),
        parent
    ),
    m_workbench(workbench),
    m_speed(1.0, 1.0),
    m_maxSpacing(1.0, 1.0)
{
    assert(workbench != NULL);
}

GridAdjustmentInstrument::~GridAdjustmentInstrument() {
    return;
}

bool GridAdjustmentInstrument::wheelEvent(QWidget *viewport, QWheelEvent *event) {
    if(event->modifiers() & Qt::ControlModifier) {
        m_currentViewport = viewport;
    } else {
        m_currentViewport.clear();
    }
    return false;
}

bool GridAdjustmentInstrument::wheelEvent(QGraphicsScene *, QGraphicsSceneWheelEvent *event) {
    QWidget *viewport = m_currentViewport.data();
    if(viewport == NULL)
        return false;

    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    if(workbench()) {
        QPointF gridMousePos = workbench()->mapper()->mapToGridF(event->scenePos());

        QSizeF spacing = workbench()->currentLayout()->cellSpacing();
        QSizeF delta = m_speed * -degrees;
        
        qreal k = 1.0;
        if(!qFuzzyIsNull(delta.width())) {
            if(delta.width() < 0) {
                k = qMin(k, spacing.width() / -delta.width());
            } else {
                k = qMin(k, (m_maxSpacing.width() - spacing.width()) / delta.width());
            }
        }
        
        if(!qFuzzyIsNull(delta.height())) {
            if(delta.height() < 0) {
                k = qMin(k, spacing.height() / -delta.height());
            } else {
                k = qMin(k, (m_maxSpacing.height() - spacing.height()) / delta.height());
            }
        }

        workbench()->currentLayout()->setCellSpacing(spacing + k * delta);

        SceneUtility::moveViewportScene(this->view(viewport), workbench()->mapper()->mapFromGridF(gridMousePos) - event->scenePos());
    }

    event->accept();
    return false;
}
