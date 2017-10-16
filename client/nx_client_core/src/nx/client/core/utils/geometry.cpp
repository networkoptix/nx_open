#include "geometry.h"

#include <QtGui/QTransform>
#include <QtQml/QtQml>

#include <cmath>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <utils/common/warnings.h>

namespace nx {
namespace client {
namespace core {
namespace utils {

QPointF Geometry::cwiseAdd(const QPointF& l, const QPointF& r)
{
    return l + r;
}

QPointF Geometry::cwiseSub(const QPointF& l, const QPointF& r)
{
    return l - r;
}

QPointF Geometry::cwiseMul(const QPointF& l, const QPointF& r)
{
    return QPointF(l.x() * r.x(), l.y() * r.y());
}

QPoint Geometry::cwiseMul(const QPoint& l, const QPoint& r)
{
    return QPoint(l.x() * r.x(), l.y() * r.y());
}

QPointF Geometry::cwiseDiv(const QPointF& l, const QPointF& r)
{
    return QPointF(l.x() / r.x(), l.y() / r.y());
}

QPointF Geometry::cwiseMul(const QPointF& l, const QSizeF& r)
{
    return QPointF(l.x() * r.width(), l.y() * r.height());
}

QPoint Geometry::cwiseMul(const QPoint& l, const QSize& r)
{
    return QPoint(l.x() * r.width(), l.y() * r.height());
}

QPointF Geometry::cwiseDiv(const QPointF& l, const QSizeF& r)
{
    return QPointF(l.x() / r.width(), l.y() / r.height());
}

QSizeF Geometry::cwiseMul(const QSizeF& l, const QSizeF& r)
{
    return QSizeF(l.width() * r.width(), l.height() * r.height());
}

QSize Geometry::cwiseMul(const QSize& l, const QSize& r)
{
    return QSize(l.width() * r.width(), l.height() * r.height());
}

QSizeF Geometry::cwiseDiv(const QSizeF& l, const QSizeF& r)
{
    return QSizeF(l.width() / r.width(), l.height() / r.height());
}

QSizeF Geometry::cwiseMin(const QSizeF& l, const QSizeF& r)
{
    return QSizeF(qMin(l.width(), r.width()), qMin(l.height(), r.height()));
}

QSizeF Geometry::cwiseMax(const QSizeF& l, const QSizeF& r)
{
    return QSizeF(qMax(l.width(), r.width()), qMax(l.height(), r.height()));
}

QMarginsF Geometry::cwiseMul(const QMarginsF& l, const QSizeF& r)
{
    return QMarginsF(
        l.left() * r.width(),
        l.top() * r.height(),
        l.right() * r.width(),
        l.bottom() * r.height());
}

QMarginsF Geometry::cwiseDiv(const QMarginsF& l, const QSizeF& r)
{
    return QMarginsF(
        l.left() / r.width(),
        l.top() / r.height(),
        l.right() / r.width(),
        l.bottom() / r.height());
}

QMarginsF Geometry::cwiseMul(const QSizeF& l, const QMarginsF& r)
{
    return QMarginsF(
        l.width() * r.left(),
        l.height() * r.top(),
        l.width() * r.right(),
        l.height() * r.bottom());
}

QMarginsF Geometry::cwiseDiv(const QSizeF& l, const QMarginsF& r)
{
    return QMarginsF(
        l.width() / r.left(),
        l.height() / r.top(),
        l.width() / r.right(),
        l.height() / r.bottom());
}

QMarginsF Geometry::cwiseMul(const QMarginsF& l, const QMarginsF& r)
{
    return QMarginsF(
        l.left() * r.left(),
        l.top() * r.top(),
        l.right() * r.right(),
        l.bottom() * r.bottom());
}

QMarginsF Geometry::cwiseDiv(const QMarginsF& l, const QMarginsF& r)
{
    return QMarginsF(
        l.left() / r.left(),
        l.top() / r.top(),
        l.right() / r.right(),
        l.bottom() / r.bottom());
}

QRectF Geometry::cwiseMul(const QRectF& l, const QSizeF& r)
{
    return QRectF(
        l.left() * r.width(),
        l.top() * r.height(),
        l.width() * r.width(),
        l.height() * r.height());
}

QRectF Geometry::cwiseDiv(const QRectF& l, const QSizeF& r)
{
    return QRectF(
        l.left() / r.width(),
        l.top() / r.height(),
        l.width() / r.width(),
        l.height() / r.height());
}

QRectF Geometry::cwiseSub(const QRectF& l, const QRectF& r)
{
    return QRectF(
        l.left() - r.left(),
        l.top() - r.top(),
        l.width() - r.width(),
        l.height() - r.height());
}

QRectF Geometry::cwiseAdd(const QRectF& l, const QRectF& r)
{
    return QRectF(
        l.left() + r.left(),
        l.top() + r.top(),
        l.width() + r.width(),
        l.height() + r.height());
}

QRectF Geometry::cwiseDiv(const QRectF& l, qreal r)
{
    return QRectF(l.left() / r, l.top() / r, l.width() / r, l.height() / r);
}

QRectF Geometry::cwiseMul(const QRectF& l, qreal r)
{
    return QRectF(l.left() * r, l.top() * r, l.width() * r, l.height() * r);
}

QMargins Geometry::cwiseSub(const QMargins& l, const QMargins& r)
{
    return QMargins(
        l.left() - r.left(), l.top() - r.top(), l.right() - r.right(), l.bottom() - r.bottom());
}

QMargins Geometry::cwiseAdd(const QMargins& l, const QMargins& r)
{
    return QMargins(
        l.left() + r.left(), l.top() + r.top(), l.right() + r.right(), l.bottom() + r.bottom());
}

int Geometry::dot(const QPoint& l, const QPoint& r)
{
    return l.x() * r.x() + l.y() * r.y();
}

QSizeF Geometry::sizeDelta(const QMarginsF& margins)
{
    return QSizeF(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize Geometry::sizeDelta(const QMargins& margins)
{
    return QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

qreal Geometry::aspectRatio(const QSizeF& size)
{
    NX_ASSERT(!qFuzzyIsNull(size.height()));
    if (qFuzzyIsNull(size.height()))
        return 0;

    return size.width() / size.height();
}

qreal Geometry::aspectRatio(const QSize& size)
{
    NX_ASSERT(size.height() != 0);
    if (size.height() == 0)
        return 0;

    return static_cast<qreal>(size.width()) / size.height();
}

qreal Geometry::aspectRatio(const QRect& rect)
{
    return aspectRatio(rect.size());
}

qreal Geometry::aspectRatio(const QRectF& rect)
{
    return aspectRatio(rect.size());
}

qreal Geometry::dotProduct(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

qreal Geometry::length(const QPointF& point)
{
    return std::sqrt(lengthSquared(point));
}

qreal Geometry::lengthSquared(const QPointF& point)
{
    return dotProduct(point, point);
}

qreal Geometry::length(const QSizeF& size)
{
    return std::sqrt(size.width() * size.width() + size.height() * size.height());
}

QPointF Geometry::normalized(const QPointF& point)
{
    return point / length(point);
}

QPointF Geometry::normal(const QPointF& point)
{
    return QPointF(point.y(), -point.x());
}

qreal Geometry::atan2(const QPointF& point)
{
    return std::atan2(point.y(), point.x());
}

QPointF Geometry::pointCentroid(const QPolygonF& polygon)
{
    QPointF result;

    int size = polygon.size() - 1;
    for (int i = 0; i < size; i++)
        result += polygon[i];

    /* Add last point only if it's not equal to the first.
     * This way both open and closed support polygons are supported. */
    if (!qFuzzyEquals(polygon[0], polygon[size]))
    {
        result += polygon[size];
        ++size;
    }

    return result / size;
}

qreal Geometry::scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode)
{
    static const qreal kNoScaleFactor = 1.0;
    if (qFuzzyIsNull(size.width()) || qFuzzyIsNull(size.height()))
        return kNoScaleFactor;

    const auto result = (mode == Qt::KeepAspectRatioByExpanding
            ? qMax(bounds.width() / size.width(), bounds.height() / size.height())
            : qMin(bounds.width() / size.width(), bounds.height() / size.height()));

    if (qFuzzyIsNull(result))
        return kNoScaleFactor;

    return result;
}

QPointF Geometry::bounded(const QPointF& pos, const QRectF& bounds)
{
    return QPointF(
        qBound(bounds.left(), pos.x(), bounds.right()),
        qBound(bounds.top(), pos.y(), bounds.bottom()));
}

QPoint Geometry::bounded(const QPoint& pos, const QRect& bounds)
{
    return QPoint(
        qBound(bounds.left(), pos.x(), bounds.right()),
        qBound(bounds.top(), pos.y(), bounds.bottom()));
}

QSizeF Geometry::bounded(const QSizeF& size, const QSizeF& maxSize, Qt::AspectRatioMode mode)
{
    if (mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if (factor >= 1.0)
        return size;

    return size * factor;
}

QSizeF Geometry::expanded(const QSizeF& size, const QSizeF& maxSize, Qt::AspectRatioMode mode)
{
    if (mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if (factor <= 1.0)
        return size;

    return size * factor;
}

QSizeF Geometry::expanded(qreal aspectRatio, const QSizeF& maxSize, Qt::AspectRatioMode mode)
{
    return scaled(aspectRatio, maxSize, mode);
}

QRectF Geometry::expanded(
    qreal aspectRatio,
    const QRectF& maxRect,
    Qt::AspectRatioMode mode,
    Qt::Alignment alignment)
{
    if (mode == Qt::IgnoreAspectRatio)
        return maxRect;

    return aligned(expanded(aspectRatio, maxRect.size(), mode), maxRect, alignment);
}

QRectF Geometry::expanded(
    qreal aspectRatio,
    const QSizeF& maxSize,
    const QPointF& center,
    Qt::AspectRatioMode mode)
{
    return expanded(
        aspectRatio, QRectF(center - toPoint(maxSize) / 2, maxSize), mode, Qt::AlignCenter);
}

QSizeF Geometry::scaled(qreal aspectRatio, const QSizeF& maxSize, Qt::AspectRatioMode mode)
{
    if (mode == Qt::IgnoreAspectRatio)
        return maxSize;

    bool expanding = mode == Qt::KeepAspectRatioByExpanding;
    bool toGreaterAspectRatio = maxSize.width() / maxSize.height() > aspectRatio;

    QSizeF result = maxSize;
    if (expanding ^ toGreaterAspectRatio)
        result.setWidth(result.height() * aspectRatio);
    else
        result.setHeight(result.width() / aspectRatio);

    return result;
}

QSizeF Geometry::scaled(const QSizeF& size, const QSizeF& maxSize, Qt::AspectRatioMode mode)
{
    return expanded(aspectRatio(size), maxSize, mode);
}

QRectF Geometry::scaled(
    const QRectF& rect,
    const QSizeF& size,
    const QPointF& fixedPoint,
    Qt::AspectRatioMode mode)
{
    QSizeF newSize = expanded(aspectRatio(rect), size, mode);

    return QRectF(
        fixedPoint - cwiseMul(cwiseDiv(fixedPoint - rect.topLeft(), rect.size()), newSize),
        newSize);
}

QRectF Geometry::scaled(const QRectF& rect, qreal scale, const QPointF& fixedPoint)
{
    QSizeF newSize = rect.size() * scale;

    return QRectF(
        fixedPoint - cwiseMul(cwiseDiv(fixedPoint - rect.topLeft(), rect.size()), newSize),
        newSize);
}

namespace {

template<typename Size, typename Rect>
Rect alignedInternal(const Size& size, const Rect& rect, Qt::Alignment alignment)
{
    Rect result;
    result.setSize(size);

    if (alignment & Qt::AlignHCenter)
        result.moveLeft(rect.left() + (rect.width() - size.width()) / 2);
    else if (alignment & Qt::AlignRight)
        result.moveRight(rect.right());
    else
        result.moveLeft(rect.left());

    if (alignment & Qt::AlignVCenter)
        result.moveTop(rect.top() + (rect.height() - size.height()) / 2);
    else if (alignment & Qt::AlignBottom)
        result.moveBottom(rect.bottom());
    else
        result.moveTop(rect.top());

    return result;
}

} // namespace

QRectF Geometry::aligned(const QSizeF& size, const QRectF& rect, Qt::Alignment alignment)
{
    return alignedInternal(size, rect, alignment);
}

QRect Geometry::aligned(const QSize& size, const QRect& rect, Qt::Alignment alignment)
{
    return alignedInternal(size, rect, alignment);
}

QRectF Geometry::dilated(const QRectF& rect, const QSizeF& amount)
{
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

QRectF Geometry::dilated(const QRectF& rect, const QMarginsF& amount)
{
    return rect.adjusted(-amount.left(), -amount.top(), amount.right(), amount.bottom());
}

QRectF Geometry::dilated(const QRectF& rect, qreal amount)
{
    return rect.adjusted(-amount, -amount, amount, amount);
}

QRect Geometry::dilated(const QRect rect, int amount)
{
    return rect.adjusted(-amount, -amount, amount, amount);
}

QSizeF Geometry::dilated(const QSizeF& size, const QMarginsF& amount)
{
    return size + sizeDelta(amount);
}

QRectF Geometry::eroded(const QRectF& rect, const QMarginsF& amount)
{
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QRectF Geometry::eroded(const QRectF& rect, qreal amount)
{
    return rect.adjusted(amount, amount, -amount, -amount);
}

QRect Geometry::eroded(const QRect& rect, int amount)
{
    return rect.adjusted(amount, amount, -amount, -amount);
}

QRect Geometry::eroded(const QRect& rect, const QMargins& amount)
{
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QSizeF Geometry::eroded(const QSizeF& size, const QMarginsF& amount)
{
    return size - sizeDelta(amount);
}

QSize Geometry::eroded(const QSize& size, const QMargins& amount)
{
    return size - sizeDelta(amount);
}

QSize Geometry::eroded(const QSize& size, int amount)
{
    return QSize(size.width() - amount, size.height() - amount);
}

QSizeF Geometry::eroded(const QSizeF& size, qreal amount)
{
    return QSizeF(size.width() - amount, size.height() - amount);
}

bool Geometry::contains(const QSizeF& size, const QSizeF& otherSize)
{
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

bool Geometry::contains(const QSize& size, const QSize& otherSize)
{
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

QRectF Geometry::movedInto(const QRectF& rect, const QRectF& target)
{
    qreal dx = 0.0;
    qreal dy = 0.0;

    if (rect.left() < target.left())
        dx += target.left() - rect.left();
    if (rect.right() > target.right())
        dx += target.right() - rect.right();

    if (rect.top() < target.top())
        dy += target.top() - rect.top();
    if (rect.bottom() > target.bottom())
        dy += target.bottom() - rect.bottom();

    return rect.translated(dx, dy);
}

QRectF Geometry::subRect(const QRectF& rect, const QRectF& relativeSubRect)
{
    return QRectF(
        rect.topLeft() + cwiseMul(relativeSubRect.topLeft(), rect.size()),
        cwiseMul(relativeSubRect.size(), rect.size()));
}

QRectF Geometry::unsubRect(const QRectF& rect, const QRectF& relativeSubRect)
{
    QSizeF size = cwiseDiv(rect.size(), relativeSubRect.size());

    return QRectF(rect.topLeft() - cwiseMul(relativeSubRect.topLeft(), size), size);
}

QRectF Geometry::toSubRect(const QRectF& rect, const QRectF& absoluteSubRect)
{
    return QRectF(
        cwiseDiv(absoluteSubRect.topLeft() - rect.topLeft(), rect.size()),
        cwiseDiv(absoluteSubRect.size(), rect.size()));
}

QPointF Geometry::corner(const QRectF& rect, Qt::Corner corner)
{
    switch (corner)
    {
        case Qt::TopLeftCorner:
            return rect.topLeft();
        case Qt::TopRightCorner:
            return rect.topRight();
        case Qt::BottomLeftCorner:
            return rect.bottomLeft();
        case Qt::BottomRightCorner:
            return rect.bottomRight();
        default:
            qnWarning("Invalid corner value '%1'.", static_cast<int>(corner));
            return rect.center();
    }
}

QPointF Geometry::closestPoint(const QRectF& rect, const QPointF& point)
{
    if (point.x() < rect.left())
    {
        if (point.y() < rect.top())
            return rect.topLeft();
        else if (point.y() < rect.bottom())
            return QPointF(rect.left(), point.y());
        else
            return rect.bottomLeft();
    }
    else if (point.x() < rect.right())
    {
        if (point.y() < rect.top())
            return QPointF(point.x(), rect.top());
        else if (point.y() < rect.bottom())
            return point;
        else
            return QPointF(point.x(), rect.bottom());
    }
    else
    {
        if (point.y() < rect.top())
            return rect.topRight();
        else if (point.y() < rect.bottom())
            return QPointF(rect.right(), point.y());
        else
            return rect.bottomRight();
    }
}

QPointF Geometry::closestPoint(const QPointF& a, const QPointF& b, const QPointF& point, qreal* t)
{
    if (qFuzzyEquals(a, b))
        return a;

    qreal k = qBound(0.0, dotProduct(point - a, b - a) / lengthSquared(b - a), 1.0);
    if (t)
        *t = k;
    return a + k * b;
}

qint64 Geometry::area(const QRect& rect)
{
    if (!rect.isValid())
        return 0;
    return rect.width() * rect.height();
}

QRectF Geometry::rotated(const QRectF& rect, qreal degrees)
{
    QPointF c = rect.center();

    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotate(degrees);
    transform.translate(-c.x(), -c.y());

    return transform.mapRect(rect);
}

QPointF Geometry::rotated(const QPointF& point, const QPointF& center, qreal degrees)
{
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(degrees);
    transform.translate(-center.x(), -center.y());
    return transform.map(point);
}

QRectF Geometry::encloseRotatedGeometry(
    const QRectF& enclosingGeometry, qreal aspectRatio, qreal rotation)
{
    /* 1. Take a rectangle with our aspect ratio */
    QRectF geom = expanded(aspectRatio, enclosingGeometry, Qt::KeepAspectRatio);

    /* 2. Rotate it */
    QRectF rotated = Geometry::rotated(geom, rotation);

    /* 3. Scale it to fit enclosing geometry */
    QSizeF scaledSize =
        bounded(expanded(rotated.size(), enclosingGeometry.size(), Qt::KeepAspectRatio),
            enclosingGeometry.size(), Qt::KeepAspectRatio);

    /* 4. Get scale factor */
    qreal scale = Geometry::scaleFactor(rotated.size(), scaledSize, Qt::KeepAspectRatio);

    /* 5. Scale original non-rotated geometry */
    geom = scaled(geom, scale, geom.center());

    return geom;
}

static QObject* createInstance(QQmlEngine* /*engine*/, QJSEngine* /*jsEngine*/)
{
    return new Geometry();
}

void Geometry::registerQmlType()
{
    qmlRegisterSingletonType<Geometry>("nx.client.core.utils", 1, 0, "Geometry", &createInstance);
}

} // namespace utils
} // namespace core
} // namespace client
} // namespace nx
