#include "geometry.h"

#include <cassert>
#include <cmath> /* For std::sqrt, std::atan2. */

#include <utils/common/warnings.h>

QPointF QnGeometry::cwiseMul(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() * r.x(), l.y() * r.y());
}

QPointF QnGeometry::cwiseDiv(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() / r.x(), l.y() / r.y());
}

QSizeF QnGeometry::cwiseMul(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() * r.width(), l.height() * r.height());
}

QSizeF QnGeometry::cwiseDiv(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() / r.width(), l.height() / r.height());
}

MarginsF QnGeometry::cwiseMul(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.right()  * r.width(),
        l.bottom() * r.height()
    );
}

MarginsF QnGeometry::cwiseDiv(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   / r.width(),
        l.top()    / r.height(),
        l.right()  / r.width(),
        l.bottom() / r.height()
    );
}

MarginsF QnGeometry::cwiseMul(const QSizeF &l, const MarginsF &r) {
    return MarginsF(
        l.width()  * r.left(),
        l.height() * r.top(),
        l.width()  * r.right(),
        l.height() * r.bottom()
    );
}

MarginsF QnGeometry::cwiseDiv(const QSizeF &l, const MarginsF &r) {
    return MarginsF(
        l.width()  / r.left(),
        l.height() / r.top(),
        l.width()  / r.right(),
        l.height() / r.bottom()
    );
}

MarginsF QnGeometry::cwiseMul(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   * r.left(), 
        l.top()    * r.top(), 
        l.right()  * r.right(), 
        l.bottom() * r.bottom()
    );
}

MarginsF QnGeometry::cwiseDiv(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   / r.left(), 
        l.top()    / r.top(), 
        l.right()  / r.right(), 
        l.bottom() / r.bottom()
    );
}

QRectF QnGeometry::cwiseMul(const QRectF &l, const QSizeF &r) {
    return QRectF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.width()  * r.width(),
        l.height() * r.height()
    );
}

QRectF QnGeometry::cwiseDiv(const QRectF &l, const QSizeF &r) {
    return QRectF(
        l.left()   / r.width(),
        l.top()    / r.height(),
        l.width()  / r.width(),
        l.height() / r.height()
    );
}

QMargins QnGeometry::cwiseSub(const QMargins &l, const QMargins &r) {
    return QMargins(
        l.left()   - r.left(), 
        l.top()    - r.top(), 
        l.right()  - r.right(), 
        l.bottom() - r.bottom()
    );
}

QMargins QnGeometry::cwiseAdd(const QMargins &l, const QMargins &r) {
    return QMargins(
        l.left()   + r.left(), 
        l.top()    + r.top(), 
        l.right()  + r.right(), 
        l.bottom() + r.bottom()
    );
}

QSizeF QnGeometry::sizeDelta(const MarginsF &margins) {
    return QSizeF(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize QnGeometry::sizeDelta(const QMargins &margins) {
    return QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

qreal QnGeometry::aspectRatio(const QSizeF &size) {
    return size.width() / size.height();
}

qreal QnGeometry::aspectRatio(const QSize &size) {
    return static_cast<qreal>(size.width()) / size.height();
}

qreal QnGeometry::aspectRatio(const QRect &rect) {
    return aspectRatio(rect.size());
}

qreal QnGeometry::aspectRatio(const QRectF &rect) {
    return aspectRatio(rect.size());
}

qreal QnGeometry::length(const QPointF &point) {
    return std::sqrt(point.x() * point.x() + point.y() * point.y());
}

qreal QnGeometry::length(const QSizeF &size) {
    return std::sqrt(size.width() * size.width() + size.height() * size.height());
}

QPointF QnGeometry::normalized(const QPointF &point) {
    return point / length(point);
}

QPointF QnGeometry::normal(const QPointF &point) {
    return QPointF(point.y(), -point.x());
}

qreal QnGeometry::atan2(const QPointF &point) {
    return std::atan2(point.y(), point.x());
}

QPointF QnGeometry::pointCentroid(const QPolygonF &polygon) {
    QPointF result;
    
    int size = polygon.size() - 1;
    for(int i = 0; i < size; i++)
        result += polygon[i];

    /* Add last point only if it's not equal to the first. 
     * This way both open and closed support polygons are supported. */
    if(!qFuzzyCompare(polygon[0], polygon[size])) {
        result += polygon[size];
        size++;
    }

    return result / size;
}

qreal QnGeometry::scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode) {
    if(mode == Qt::KeepAspectRatioByExpanding) {
        return qMax(bounds.width() / size.width(), bounds.height() / size.height());
    } else {
        return qMin(bounds.width() / size.width(), bounds.height() / size.height());
    }
}

QPoint QnGeometry::bounded(const QPoint &pos, const QRect &bounds) {
    return QPoint(
        qBound(bounds.left(), pos.x(), bounds.right()),
        qBound(bounds.top(), pos.y(), bounds.bottom())
    );
}

QSizeF QnGeometry::bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if(factor >= 1.0)
        return size;

    return size * factor;
}

QSizeF QnGeometry::expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(minSize);

    qreal factor = scaleFactor(size, minSize, mode);
    if(factor <= 1.0)
        return size;

    return size * factor;
}

QSizeF QnGeometry::expanded(qreal aspectRatio, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return minSize;

    bool expanding = mode == Qt::KeepAspectRatioByExpanding;
    bool toGreaterAspectRatio = minSize.width() / minSize.height() > aspectRatio;

    QSizeF result = minSize;
    if(expanding ^ toGreaterAspectRatio) {
        result.setWidth(result.height() * aspectRatio);
    } else {
        result.setHeight(result.width() / aspectRatio);
    }
    return result;
}

QRectF QnGeometry::expanded(qreal aspectRatio, const QRectF &minRect, Qt::AspectRatioMode mode, Qt::Alignment alignment) {
    if(mode == Qt::IgnoreAspectRatio)
        return minRect;

    QSizeF newSize = expanded(aspectRatio, minRect.size(), mode);
    QRectF result;
    result.setSize(newSize);

    if(alignment & Qt::AlignHCenter) {
        result.moveLeft(minRect.left() + (minRect.width() - newSize.width()) / 2);
    } else if(alignment & Qt::AlignRight) {
        result.moveRight(minRect.right());
    } else {
        result.moveLeft(minRect.left());
    }

    if(alignment & Qt::AlignVCenter) {
        result.moveTop(minRect.top() + (minRect.height() - newSize.height()) / 2);
    } else if(alignment & Qt::AlignBottom) {
        result.moveBottom(minRect.bottom());
    } else {
        result.moveTop(minRect.top());
    }

    return result;
}

QRectF QnGeometry::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

QRectF QnGeometry::dilated(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(-amount.left(), -amount.top(), amount.right(), amount.bottom());
}

QRectF QnGeometry::dilated(const QRectF &rect, qreal amount) {
    return rect.adjusted(-amount, -amount, amount, amount);
}

QSizeF QnGeometry::dilated(const QSizeF &size, const MarginsF &amount) {
    return size + sizeDelta(amount);
}

QRectF QnGeometry::eroded(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QRectF QnGeometry::eroded(const QRectF &rect, qreal amount) {
    return rect.adjusted(amount, amount, -amount, -amount);
}

QRect QnGeometry::eroded(const QRect &rect, const QMargins &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QSizeF QnGeometry::eroded(const QSizeF &size, const MarginsF &amount) {
    return size - sizeDelta(amount);
}

QSize QnGeometry::eroded(const QSize &size, const QMargins &amount) {
    return size - sizeDelta(amount);
}

bool QnGeometry::contains(const QSizeF &size, const QSizeF &otherSize) {
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

bool QnGeometry::contains(const QSize &size, const QSize &otherSize) {
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

