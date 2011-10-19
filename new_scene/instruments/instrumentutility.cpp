#include "instrumentutility.h"
#include <QGraphicsView>

QRectF InstrumentUtility::mapRectToScene(QGraphicsView *view, const QRect &rect) {
    return view->viewportTransform().inverted().mapRect(rect);
}

QRect InstrumentUtility::mapRectFromScene(QGraphicsView *view, const QRectF &rect) {
    return view->viewportTransform().mapRect(rect).toRect();
}

QSizeF InstrumentUtility::bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal ratio = qMin(maxSize.width() / size.width(), maxSize.height() / size.height());
    if(ratio >= 1.0)
        return size;

    return size * ratio;
}

QSizeF InstrumentUtility::expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(minSize);

    qreal ratio = qMax(minSize.width() / size.width(), minSize.height() / size.height());
    if(ratio <= 1.0)
        return size;

    return size * ratio;
}

QRectF InstrumentUtility::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}
