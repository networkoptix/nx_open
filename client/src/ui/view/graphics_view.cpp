#include "graphics_view.h"
#include <utils/common/warnings.h>

QnLayerPainter::QnLayerPainter(): m_view(NULL), m_layer(static_cast<QGraphicsScene::SceneLayer>(0)) {}

QnLayerPainter::~QnLayerPainter() {
    ensureUninstalled();
}

void QnLayerPainter::ensureUninstalled() {
    if(m_view != NULL)
        m_view->uninstallLayerPainter(this);
}


QnGraphicsView::QnGraphicsView(QGraphicsScene *scene, QWidget * parent):
    QGraphicsView(scene, parent)
{}

QnGraphicsView::~QnGraphicsView() {
    while(!m_backgroundPainters.empty())
        uninstallLayerPainter(m_backgroundPainters[0]);

    while(!m_foregroundPainters.empty())
        uninstallLayerPainter(m_foregroundPainters[0]);
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
}

void QnGraphicsView::uninstallLayerPainter(QnLayerPainter *painter) {
    if(painter->view() == NULL)
        return; /* Do nothing is it's not installed. */

    if(painter->view() != this) {
        qnWarning("Given layer painter is installed into another view, cannot uninstall.");
        return;
    }

    (painter->layer() == QGraphicsScene::ForegroundLayer ? m_foregroundPainters : m_backgroundPainters).removeOne(painter);
    painter->m_layer = static_cast<QGraphicsScene::SceneLayer>(0);
    painter->m_view = NULL;
}

void QnGraphicsView::drawBackground(QPainter *painter, const QRectF &rect) {
    base_type::drawBackground(painter, rect);

    foreach(QnLayerPainter *layerPainter, m_backgroundPainters)
        layerPainter->drawLayer(painter, rect);
}

void QnGraphicsView::drawForeground(QPainter *painter, const QRectF &rect) {
    base_type::drawForeground(painter, rect);

    foreach(QnLayerPainter *layerPainter, m_foregroundPainters)
        layerPainter->drawLayer(painter, rect);
}

