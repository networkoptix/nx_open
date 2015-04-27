#include "geometry.h"

#include <cassert>
#include <cmath> /* For std::sqrt, std::atan2. */

#include <utils/math/fuzzy.h>
#include <utils/common/warnings.h>

QPointF QnGeometry::cwiseMul(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() * r.x(), l.y() * r.y());
}

QPoint QnGeometry::cwiseMul(const QPoint &l, const QPoint &r) {
    return QPoint(l.x() * r.x(), l.y() * r.y());
}

QPointF QnGeometry::cwiseDiv(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() / r.x(), l.y() / r.y());
}

QPointF QnGeometry::cwiseMul(const QPointF &l, const QSizeF &r) {
    return QPointF(l.x() * r.width(), l.y() * r.height());
}

QPoint QnGeometry::cwiseMul(const QPoint &l, const QSize &r) {
    return QPoint(l.x() * r.width(), l.y() * r.height());
}

QPointF QnGeometry::cwiseDiv(const QPointF &l, const QSizeF &r) {
    return QPointF(l.x() / r.width(), l.y() / r.height());
}

QSizeF QnGeometry::cwiseMul(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() * r.width(), l.height() * r.height());
}

QSize QnGeometry::cwiseMul(const QSize &l, const QSize &r) {
    return QSize(l.width() * r.width(), l.height() * r.height());
}

QSizeF QnGeometry::cwiseDiv(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() / r.width(), l.height() / r.height());
}

QSizeF QnGeometry::cwiseMin(const QSizeF &l, const QSizeF &r) {
    return QSizeF(qMin(l.width(), r.width()), qMin(l.height(), r.height()));
}

QSizeF QnGeometry::cwiseMax(const QSizeF &l, const QSizeF &r) {
    return QSizeF(qMax(l.width(), r.width()), qMax(l.height(), r.height()));
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

QRectF QnGeometry::cwiseSub(const QRectF &l, const QRectF &r) {
    return QRectF(
        l.left() - r.left(),
        l.top() - r.top(),
        l.width() - r.width(),
        l.height() - r.height()
    );
}

QRectF QnGeometry::cwiseAdd(const QRectF &l, const QRectF &r) {
    return QRectF(
        l.left() + r.left(),
        l.top() + r.top(),
        l.width() + r.width(),
        l.height() + r.height()
    );
}

QRectF QnGeometry::cwiseDiv(const QRectF &l, qreal r) {
    return QRectF(
        l.left() / r,
        l.top() / r,
        l.width() / r,
        l.height() / r
    );
}

QRectF QnGeometry::cwiseMul(const QRectF &l, qreal r) {
    return QRectF(
        l.left() * r,
        l.top() * r,
        l.width() * r,
        l.height() * r
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

int QnGeometry::dot(const QPoint &l, const QPoint &r) {
    return l.x() * r.x() + l.y() * r.y();
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

qreal QnGeometry::dotProduct(const QPointF &a, const QPointF &b) {
    return a.x() * b.x() + a.y() * b.y();
}

qreal QnGeometry::length(const QPointF &point) {
    return std::sqrt(lengthSquared(point));
}

qreal QnGeometry::lengthSquared(const QPointF &point) {
    return dotProduct(point, point);
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
    if(!qFuzzyEquals(polygon[0], polygon[size])) {
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

QPointF QnGeometry::bounded(const QPointF &pos, const QRectF &bounds) {
    return QPointF(
        qBound(bounds.left(), pos.x(), bounds.right()),
        qBound(bounds.top(), pos.y(), bounds.bottom())
    );
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

QSizeF QnGeometry::expanded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if(factor <= 1.0)
        return size;

    return size * factor;
}

QSizeF QnGeometry::expanded(qreal aspectRatio, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return maxSize;

    bool expanding = mode == Qt::KeepAspectRatioByExpanding;
    bool toGreaterAspectRatio = maxSize.width() / maxSize.height() > aspectRatio;

    QSizeF result = maxSize;
    if(expanding ^ toGreaterAspectRatio) {
        result.setWidth(result.height() * aspectRatio);
    } else {
        result.setHeight(result.width() / aspectRatio);
    }
    return result;
}

QRectF QnGeometry::expanded(qreal aspectRatio, const QRectF &maxRect, Qt::AspectRatioMode mode, Qt::Alignment alignment) {
    if(mode == Qt::IgnoreAspectRatio)
        return maxRect;

    return aligned(expanded(aspectRatio, maxRect.size(), mode), maxRect, alignment);
}

QRectF QnGeometry::expanded(qreal aspectRatio, const QSizeF &maxSize, const QPointF &center, Qt::AspectRatioMode mode) {
    return expanded(aspectRatio, QRectF(center - toPoint(maxSize) / 2, maxSize), mode, Qt::AlignCenter);
}

QRectF QnGeometry::scaled(const QRectF &rect, const QSizeF &size, const QPointF &fixedPoint, Qt::AspectRatioMode mode) {
    QSizeF newSize = expanded(aspectRatio(rect), size, mode);

    return QRectF(
        fixedPoint - cwiseMul(cwiseDiv(fixedPoint - rect.topLeft(), rect.size()), newSize),
        newSize
                );
}

QRectF QnGeometry::scaled(const QRectF &rect, qreal scale, const QPointF &fixedPoint) {
    QSizeF newSize = rect.size() * scale;

    return QRectF(fixedPoint - cwiseMul(cwiseDiv(fixedPoint - rect.topLeft(), rect.size()), newSize), newSize);
}

namespace {
    template<class Size, class Rect>
    Rect alignedInternal(const Size &size, const Rect &rect, Qt::Alignment alignment) {
        Rect result;
        result.setSize(size);

        if(alignment & Qt::AlignHCenter) {
            result.moveLeft(rect.left() + (rect.width() - size.width()) / 2);
        } else if(alignment & Qt::AlignRight) {
            result.moveRight(rect.right());
        } else {
            result.moveLeft(rect.left());
        }

        if(alignment & Qt::AlignVCenter) {
            result.moveTop(rect.top() + (rect.height() - size.height()) / 2);
        } else if(alignment & Qt::AlignBottom) {
            result.moveBottom(rect.bottom());
        } else {
            result.moveTop(rect.top());
        }

        return result;
    }
} // anonymous namespace

QRectF QnGeometry::aligned(const QSizeF &size, const QRectF &rect, Qt::Alignment alignment) {
    return alignedInternal(size, rect, alignment);
}

QRect QnGeometry::aligned(const QSize &size, const QRect &rect, Qt::Alignment alignment) {
    return alignedInternal(size, rect, alignment);
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

QRect QnGeometry::dilated(const QRect rect, int amount) {
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

QRect QnGeometry::eroded(const QRect &rect, int amount) {
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

QRectF QnGeometry::movedInto(const QRectF &rect, const QRectF &target) {
    qreal dx = 0.0, dy = 0.0;

    if(rect.left() < target.left())
        dx += target.left() - rect.left();
    if(rect.right() > target.right())
        dx += target.right() - rect.right();

    if(rect.top() < target.top())
        dy += target.top() - rect.top();
    if(rect.bottom() > target.bottom())
        dy += target.bottom() - rect.bottom();

    return rect.translated(dx, dy);
}

QRectF QnGeometry::subRect(const QRectF &rect, const QRectF &relativeSubRect) {
    return QRectF(
        rect.topLeft() + cwiseMul(relativeSubRect.topLeft(), rect.size()),
        cwiseMul(relativeSubRect.size(), rect.size())
    );
}

QRectF QnGeometry::unsubRect(const QRectF &rect, const QRectF &relativeSubRect) {
    QSizeF size = cwiseDiv(rect.size(), relativeSubRect.size());

    return QRectF(
        rect.topLeft() - cwiseMul(relativeSubRect.topLeft(), size),
        size
    );
}

QRectF QnGeometry::toSubRect(const QRectF &rect, const QRectF &absoluteSubRect) {
    return QRectF(
        cwiseDiv(absoluteSubRect.topLeft() - rect.topLeft(), rect.size()),
        cwiseDiv(absoluteSubRect.size(), rect.size())
    );
}

QPointF QnGeometry::corner(const QRectF &rect, Qn::Corner corner) {
    switch(corner) {
        case Qn::TopLeftCorner: 
            return rect.topLeft();
        case Qn::TopRightCorner: 
            return rect.topRight();
        case Qn::BottomLeftCorner: 
            return rect.bottomLeft();
        case Qn::BottomRightCorner: 
            return rect.bottomRight();
        case Qn::NoCorner: 
            return rect.center();
        default:
            qnWarning("Invalid corner value '%1'.", static_cast<int>(corner));
            return rect.center();
    }
}

QPointF QnGeometry::closestPoint(const QRectF &rect, const QPointF &point) {
    if(point.x() < rect.left()) {
        if(point.y() < rect.top()) {
            return rect.topLeft();
        } else if(point.y() < rect.bottom()) {
            return QPointF(rect.left(), point.y());
        } else {
            return rect.bottomLeft();
        }
    } else if(point.x() < rect.right()) {
        if(point.y() < rect.top()) {
            return QPointF(point.x(), rect.top());
        } else if(point.y() < rect.bottom()) {
            return point;
        } else {
            return QPointF(point.x(), rect.bottom());
        }
    } else {
        if(point.y() < rect.top()) {
            return rect.topRight();
        } else if(point.y() < rect.bottom()) {
            return QPointF(rect.right(), point.y());
        } else {
            return rect.bottomRight();
        }
    }
}

QPointF QnGeometry::closestPoint(const QPointF &a, const QPointF &b, const QPointF &point, qreal *t) {
    if(qFuzzyEquals(a, b))
        return a;

    qreal k = qBound(0.0, dotProduct(point - a, b - a) / lengthSquared(b - a), 1.0);
    if(t)
        *t = k;
    return a + k * b;
}

qint64 QnGeometry::area(const QRect &rect) {
    if (!rect.isValid())
        return 0;
    return rect.width() * rect.height();
}

QRectF QnGeometry::rotated(const QRectF &rect, qreal degrees) {
    QPointF c = rect.center();

    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotate(degrees);
    transform.translate(-c.x(), -c.y());

    return transform.mapRect(rect);
}

QPointF QnGeometry::rotated(const QPointF &point, const QPointF &center, qreal degrees) {
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(degrees);
    transform.translate(-center.x(), -center.y());
    return transform.map(point);
}

QRectF QnGeometry::encloseRotatedGeometry(const QRectF &enclosingGeometry, qreal aspectRatio, qreal rotation) {
    /* 1. Take a rectangle with our aspect ratio */
    QRectF geom = expanded(aspectRatio, enclosingGeometry, Qt::KeepAspectRatio);

    /* 2. Rotate it */
    QRectF rotated = QnGeometry::rotated(geom, rotation);

    /* 3. Scale it to fit enclosing geometry */
    QSizeF scaledSize = bounded(expanded(rotated.size(), enclosingGeometry.size(), Qt::KeepAspectRatio), enclosingGeometry.size(), Qt::KeepAspectRatio);

    /* 4. Get scale factor */
    qreal scale = QnGeometry::scaleFactor(rotated.size(), scaledSize, Qt::KeepAspectRatio);

    /* 5. Scale original non-rotated geometry */
    geom = scaled(geom, scale, geom.center());

    return geom;
}
