#include "viewport_bound_widget.h"
#include <cmath> /* For std::sqrt. */
#include <QGraphicsView>

QnViewportBoundWidget::QnViewportBoundWidget(QGraphicsItem *parent):
    QGraphicsWidget(parent)
{}

QnViewportBoundWidget::~QnViewportBoundWidget() {
    return;
}

void QnViewportBoundWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QGraphicsView *view = widget ? dynamic_cast<QGraphicsView *>(widget->parent()) : NULL;
    if(!view)
        view = m_lastView.data();
    if(!view && scene() && !scene()->views().empty())
        view = scene()->views()[0];
    if(!view) {
        base_type::paint(painter, option, widget);
        return;
    }
    m_lastView = view;

    QTransform sceneToViewport = view->viewportTransform();
    if(!qFuzzyCompare(sceneToViewport, m_sceneToViewport)) {
        m_sceneToViewport = sceneToViewport;

        /* Assume affine transform that does not change x/y scale separately. */
        qreal scale = 1.0 / std::sqrt(sceneToViewport.m11() * sceneToViewport.m11() + sceneToViewport.m12() * sceneToViewport.m12());

        QRectF geometry = QRectF(QPointF(0, 0), size() / scale);
        setGeometry(geometry);

        QSizeF resultingSize = size();
        if(resultingSize.width() > geometry.width() || resultingSize.height() > geometry.height()) {
            qreal k = qMax(resultingSize.width() / geometry.width(), resultingSize.height() / geometry.height());

            geometry.setSize(geometry.size() * k);
            scale /= k;

            setGeometry(geometry);
        }

        setTransform(QTransform::fromScale(scale, scale));
    }

    base_type::paint(painter, option, widget);
}
