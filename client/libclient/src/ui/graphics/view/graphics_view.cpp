#include "graphics_view.h"

#include <QtOpenGL/QtOpenGL>

#include <utils/common/warnings.h>
#include <utils/common/performance.h>

#ifdef QN_GRAPHICS_VIEW_DEBUG_PERFORMANCE
#   include <QtCore/QDateTime>
#endif

namespace {
    class PaintDevice: public QPaintDevice {
    public:
        QPaintDevice *invokeRedirected(QPoint *offset) const {
            return redirected(offset);
        }
    };

    PaintDevice *open(QPaintDevice *paintDevice) {
        return static_cast<PaintDevice *>(paintDevice);
    }

} // anonymous namespace


QnLayerPainter::QnLayerPainter(): 
    m_view(NULL),
    m_layer(static_cast<QGraphicsScene::SceneLayer>(0)),
    m_enabled(true)
{}

QnLayerPainter::~QnLayerPainter() {
    ensureUninstalled();
}

void QnLayerPainter::ensureUninstalled() {
    if(m_view != NULL)
        m_view->uninstallLayerPainter(this);
}

bool QnLayerPainter::isEnabled() const {
    return m_enabled;
}

void QnLayerPainter::setEnabled(bool enabled) {
    m_enabled = enabled;
}


QnGraphicsView::QnGraphicsView(QGraphicsScene *scene, QWidget * parent):
    QGraphicsView(scene, parent),
    m_paintFlags(0),
    m_behaviorFlags(0)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

QnGraphicsView::~QnGraphicsView() {
    while(!m_backgroundPainters.empty())
        uninstallLayerPainter(m_backgroundPainters[0]);

    while(!m_foregroundPainters.empty())
        uninstallLayerPainter(m_foregroundPainters[0]);
}

void QnGraphicsView::setPaintFlags(PaintFlags paintFlags) {
    m_paintFlags = paintFlags;
}

void QnGraphicsView::setBehaviorFlags(BehaviorFlags behaviorFlags) {
    m_behaviorFlags = behaviorFlags;
}

void QnGraphicsView::installLayerPainter(QnLayerPainter *painter, QGraphicsScene::SceneLayer layer) {
    if(painter->view() != NULL) {
        qnWarning("Cannot install a layer painter that is already installed.");
        return;
    }

    if(layer != QGraphicsScene::ForegroundLayer && layer != QGraphicsScene::BackgroundLayer) {
        qnWarning("Installing a layer painter into layer %1 is not supported", static_cast<int>(layer));
        return;
    }

    painter->m_view = this;
    painter->m_layer = layer;
    (layer == QGraphicsScene::ForegroundLayer ? m_foregroundPainters : m_backgroundPainters).push_back(painter);
    painter->installedNotify();
}

void QnGraphicsView::uninstallLayerPainter(QnLayerPainter *painter) {
    if(painter->view() == NULL)
        return; /* Do nothing is it's not installed. */

    if(painter->view() != this) {
        qnWarning("Given layer painter is installed into another view, cannot uninstall.");
        return;
    }

    painter->aboutToBeUninstalledNotify();
    (painter->layer() == QGraphicsScene::ForegroundLayer ? m_foregroundPainters : m_backgroundPainters).removeOne(painter);
    painter->m_layer = static_cast<QGraphicsScene::SceneLayer>(0);
    painter->m_view = NULL;
}

void QnGraphicsView::showEvent(QShowEvent *event) {
    if(m_behaviorFlags & CenterOnShow) {
        base_type::showEvent(event);
    } else {
        QAbstractScrollArea::showEvent(event); /* Skip base's implementation. */
    }
}

bool QnGraphicsView::isInRedirectedPaint() const {
    QPoint offset;
    return open(viewport())->invokeRedirected(&offset) != viewport();
}

void QnGraphicsView::paintEvent(QPaintEvent *event) {
    if(isInRedirectedPaint())
        return;

#ifdef QN_GRAPHICS_VIEW_DEBUG_PERFORMANCE
    qint64 frequency = QnPerformance::currentCpuFrequency();
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    qint64 startCycles = QnPerformance::currentThreadCycles();
#endif

    base_type::paintEvent(event);

#ifdef QN_GRAPHICS_VIEW_DEBUG_PERFORMANCE
    qint64 deltaTime = QDateTime::currentMSecsSinceEpoch() - startTime;
    qint64 deltaCycles = QnPerformance::currentThreadCycles() - startCycles;

    qreal deltaCpuTime = deltaCycles / (frequency / 1000.0);
    if(deltaCpuTime > 1.0)
        qDebug() << QString("VIEWPORT PAINT: %1ms real time; %2ms cpu time (%3 cycles)").arg(deltaTime, 5).arg(deltaCpuTime, 8, 'g', 5).arg(deltaCycles, 8);
#endif
}

void QnGraphicsView::wheelEvent(QWheelEvent *event) {
    if(m_behaviorFlags & InvokeInheritedMouseWheel) {
        base_type::wheelEvent(event);
    } else {
        /* Copied from QGraphicsView implementation.
         * The only difference: we don't invoke QAbstractScrollArea implementation. */
        if (!scene() || !isInteractive())
            return;

        event->ignore();

        QGraphicsSceneWheelEvent wheelEvent(QEvent::GraphicsSceneWheel);
        wheelEvent.setWidget(viewport());
        wheelEvent.setScenePos(mapToScene(event->pos()));
        wheelEvent.setScreenPos(event->globalPos());
        wheelEvent.setButtons(event->buttons());
        wheelEvent.setModifiers(event->modifiers());
        wheelEvent.setDelta(event->delta());
        wheelEvent.setOrientation(event->orientation());
        wheelEvent.setAccepted(false);
        QApplication::sendEvent(scene(), &wheelEvent);
        event->setAccepted(wheelEvent.isAccepted());
    }
}

void QnGraphicsView::drawBackground(QPainter *painter, const QRectF &rect) {
    if(m_paintFlags & PaintInheritedBackround)
        base_type::drawBackground(painter, rect);

    foreach(QnLayerPainter *layerPainter, m_backgroundPainters)
        layerPainter->drawLayer(painter, rect);
}

void QnGraphicsView::drawForeground(QPainter *painter, const QRectF &rect) {
    if(m_paintFlags & PaintInheritedForeground)
        base_type::drawForeground(painter, rect);

    foreach(QnLayerPainter *layerPainter, m_foregroundPainters)
        layerPainter->drawLayer(painter, rect);
}

