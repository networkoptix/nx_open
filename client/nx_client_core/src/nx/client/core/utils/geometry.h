#pragma once

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtGui/QPolygon>

namespace nx {
namespace client {
namespace core {

class Geometry: public QObject
{
    Q_OBJECT

public:
    /* Some coefficient-wise arithmetic functions follow. */
    Q_INVOKABLE static QPointF cwiseAdd(const QPointF& l, const QPointF& r);
    Q_INVOKABLE static QPointF cwiseSub(const QPointF& l, const QPointF& r);
    Q_INVOKABLE static QPointF cwiseMul(const QPointF& l, const QPointF& r);
    Q_INVOKABLE static QPoint cwiseMul(const QPoint& l, const QPoint& r);
    Q_INVOKABLE static QPointF cwiseDiv(const QPointF& l, const QPointF& r);
    Q_INVOKABLE static QPointF cwiseMul(const QPointF& l, const QSizeF& r);
    Q_INVOKABLE static QPoint cwiseMul(const QPoint& l, const QSize& r);
    Q_INVOKABLE static QPointF cwiseDiv(const QPointF& l, const QSizeF& r);
    Q_INVOKABLE static QSizeF cwiseAdd(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QSizeF cwiseSub(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QSizeF cwiseMul(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QSize cwiseMul(const QSize& l, const QSize& r);
    Q_INVOKABLE static QSizeF cwiseDiv(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QSizeF cwiseMin(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QSizeF cwiseMax(const QSizeF& l, const QSizeF& r);
    Q_INVOKABLE static QMarginsF cwiseMul(const QMarginsF& l, const QSizeF& r);
    Q_INVOKABLE static QMarginsF cwiseDiv(const QMarginsF& l, const QSizeF& r);
    Q_INVOKABLE static QMarginsF cwiseMul(const QSizeF& l, const QMarginsF& r);
    Q_INVOKABLE static QMarginsF cwiseDiv(const QSizeF& l, const QMarginsF& r);
    Q_INVOKABLE static QMarginsF cwiseMul(const QMarginsF& l, const QMarginsF& r);
    Q_INVOKABLE static QMarginsF cwiseDiv(const QMarginsF& l, const QMarginsF& r);
    Q_INVOKABLE static QRectF cwiseMul(const QRectF& l, const QSizeF& r);
    Q_INVOKABLE static QRectF cwiseDiv(const QRectF& l, const QSizeF& r);
    Q_INVOKABLE static QRectF cwiseSub(const QRectF& l, const QRectF& r);
    Q_INVOKABLE static QRectF cwiseAdd(const QRectF& l, const QRectF& r);
    Q_INVOKABLE static QRectF cwiseDiv(const QRectF& l, qreal r);
    Q_INVOKABLE static QRectF cwiseMul(const QRectF& l, qreal r);
    Q_INVOKABLE static QMargins cwiseSub(const QMargins& l, const QMargins& r);
    Q_INVOKABLE static QMargins cwiseAdd(const QMargins& l, const QMargins& r);

    Q_INVOKABLE static int dot(const QPoint& l, const QPoint& r);

    /**
     * @return Amount by which the size of some rectangle will change if the given margins are
     *     applied to it.
     */
    Q_INVOKABLE static QSizeF sizeDelta(const QMarginsF& margins);

    /**
     * @return Amount by which the size of some rectangle will change if the given margins are
     *     applied to it.
     */
    Q_INVOKABLE static QSize sizeDelta(const QMargins& margins);

    /**
     * @return Aspect ratio of the given size.
     */
    Q_INVOKABLE static qreal aspectRatio(const QSizeF& size);

    /**
     * @return Aspect ratio of the given size.
     */
    Q_INVOKABLE static qreal aspectRatio(const QSize& size);

    /**
     * @return Aspect ratio of the given rectangle.
     */
    Q_INVOKABLE static qreal aspectRatio(const QRect& rect);

    /**
     * @return Aspect ratio of the given rectangle.
     */
    Q_INVOKABLE static qreal aspectRatio(const QRectF& rect);

    /**
     * @return Aspect ratio of the given size or defaultValue if height is equal to 0.
     */
    Q_INVOKABLE static qreal aspectRatio(const QSize& size, qreal defaultValue);

    /**
     * @return Aspect ratio of the given size or defaultValue if height is equal to 0.
     */
    Q_INVOKABLE static qreal aspectRatio(const QSizeF& size, qreal defaultValue);

    /**
     * @return Aspect ratio of the given rect or defaultValue if height is equal to 0.
     */
    Q_INVOKABLE static qreal aspectRatio(const QRect& rect, qreal defaultValue);

    /**
     * @return Aspect ratio of the given rect or defaultValue if height is equal to 0.
     */
    Q_INVOKABLE static qreal aspectRatio(const QRectF& rect, qreal defaultValue);

    /**
     * @return Given size converted to a point.
     */
    Q_INVOKABLE static QPointF toPoint(const QSizeF& size)
    {
        return QPointF(size.width(), size.height());
    }

    /**
     * @return Given size converted to a point.
     */
    Q_INVOKABLE static QPoint toPoint(const QSize& size)
    {
        return QPoint(size.width(), size.height());
    }

    /**
     * @return Given point converted to a size.
     */
    Q_INVOKABLE static QSizeF toSize(const QPointF& point)
    {
        return QSizeF(point.x(), point.y());
    }

    /**
     * @return Given point converted to a size.
     */
    Q_INVOKABLE static QSize toSize(const QPoint& point)
    {
        return QSize(point.x(), point.y());
    }

    Q_INVOKABLE static qreal dotProduct(const QPointF& a, const QPointF& b);

    /**
     * @param point Point, treated as a vector.
     * @return Length of the given vector.
     */
    Q_INVOKABLE static qreal length(const QPointF& point);

    /**
     * @param point Point, treated as a vector.
     * @return Squared length of the given vector.
     */
    Q_INVOKABLE static qreal lengthSquared(const QPointF& point);

    /**
     * @param size Size, treated as a vector.
     * @return Length of the given vector.
     */
    Q_INVOKABLE static qreal length(const QSizeF& size);

    /**
     * @param point Point, treated as a vector.
     * @param Unit vector pointing in the same direction as the given vector.
     */
    Q_INVOKABLE static QPointF normalized(const QPointF& point);

    /**
     * @param point Point, treated as a vector.
     * @return Normal of the same length for the given vector.
     */
    Q_INVOKABLE static QPointF normal(const QPointF& point);

    /**
     * @param point Point, treated as a vector.
     * @return Angle between the positive x-axis and the given vector, in radians.
     *     The angle is positive for counter-clockwise angles (upper half-plane, y > 0),
     *     and negative for clockwise angles (lower half-plane, y < 0).
     */
    Q_INVOKABLE static qreal atan2(const QPointF& point);

    /**
     * @param polygon Polygon.
     * @return Centroid of the set of vertices of the given polygon.
     *
     * Note that this NOT centroid of the polygon itself.
     */
    Q_INVOKABLE static QPointF pointCentroid(const QPolygonF& polygon);

    /**
     * Calculates the factor by which the given size must be scaled to fit into the given bounds.
     *
     * Note that Qt::IgnoreAspectRatio mode is treated the same way as Qt::KeepAspectRatio mode.
     *
     * @param size Size to scale.
     * @param bounds Size bounds.
     * @param mode Aspect ratio mode.
     * @return Scale factor.
     */
    Q_INVOKABLE static qreal scaleFactor(
        QSizeF size,
        QSizeF bounds,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    Q_INVOKABLE static QPointF bounded(const QPointF& pos, const QRectF& bounds);
    Q_INVOKABLE static QPoint bounded(const QPoint& pos, const QRect& bounds);

    /**
     * Bounds the given size, so that a rectangle of size maxSize would include
     *     a rectangle of bounded size.
     *
     * @param size Size to bound.
     * @param maxSize Maximal size.
     * @param mode Aspect ratio mode.
     */
    Q_INVOKABLE static QSizeF bounded(
        const QSizeF& size,
        const QSizeF& maxSize,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    /**
     * Expands the given size to given maxSize.
     *
     * @param size Size to expand.
     * @param maxSize Maximal size.
     * @param mode Aspect ratio mode.
     */
    Q_INVOKABLE static QSizeF expanded(
        const QSizeF& size,
        const QSizeF& maxSize,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    /**
     * Expands an infinitely small size with the given aspect ratio to the given maximal size.
     *
     * @param aspectRatio Aspect ratio.
     * @param maxSize Maximal size.
     * @param mode Aspect ratio mode.
     */
    Q_INVOKABLE static QSizeF expanded(
        qreal aspectRatio,
        const QSizeF& maxSize,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    /**
     * Expands an infinitely small rectangle with the given aspect ratio to the given maximal
     *     rectangle.
     *
     * @param aspectRatio Aspect ratio.
     * @param maxRect Maximal rectangle.
     * @param mode Aspect ratio mode.
     * @param alignment Alignment of the result relative to maximal rectangle.
     */
    Q_INVOKABLE static QRectF expanded(
        qreal aspectRatio,
        const QRectF& maxRect,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio,
        Qt::Alignment alignment = Qt::AlignCenter);

    /**
     * Expands an infinitely small rectanglr with the given aspect ratio to the given maximal
     *     rectangle.
     *
     * @param aspectRatio Aspect ratio.
     * @param maxSize Maximal size.
     * @param center Center of the given rectangle.
     * @param mode Aspect ratio mode.
     */
    Q_INVOKABLE static QRectF expanded(
        qreal aspectRatio,
        const QSizeF& maxSize,
        const QPointF& center,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    Q_INVOKABLE static QSizeF scaled(
        qreal aspectRatio,
        const QSizeF& maxSize,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    Q_INVOKABLE static QSizeF scaled(
        const QSizeF& size,
        const QSizeF& maxSize,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    Q_INVOKABLE static QRectF scaled(
        const QRectF& rect,
        const QSizeF& size,
        const QPointF& fixedPoint,
        Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

    Q_INVOKABLE static QRectF scaled(
        const QRectF& rect,
        qreal scale,
        const QPointF& fixedPoint);

    Q_INVOKABLE static QRectF aligned(
        const QSizeF& size,
        const QRectF& rect,
        Qt::Alignment alignment = Qt::AlignCenter);

    Q_INVOKABLE static QRect aligned(
        const QSize& size,
        const QRect& rect,
        Qt::Alignment alignment = Qt::AlignCenter);

    /** Dilates the given size by the given amount. */
    Q_INVOKABLE static QRectF dilated(const QRectF& rect, const QSizeF& amount);

    /** Dilates the given size by the given amount. */
    Q_INVOKABLE static QRectF dilated(const QRectF& rect, const QMarginsF& amount);

    /** Dilates the given size by the given amount. */
    Q_INVOKABLE static QRectF dilated(const QRectF& rect, qreal amount);

    /** Dilates the given size by the given amount. */
    Q_INVOKABLE static QRect dilated(const QRect rect, int amount);

    /** Dilates the given size by the given amount. */
    Q_INVOKABLE static QSizeF dilated(const QSizeF& size, const QMarginsF& amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QRectF eroded(const QRectF& rect, const QMarginsF& amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QRectF eroded(const QRectF& rect, qreal amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QRect eroded(const QRect& rect, int amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QRect eroded(const QRect& rect, const QMargins& amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QSizeF eroded(const QSizeF& size, const QMarginsF& amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QSize eroded(const QSize& size, const QMargins& amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QSize eroded(const QSize& size, int amount);

    /** Erodes the given size by the given amount. */
    Q_INVOKABLE static QSizeF eroded(const QSizeF& size, qreal amount);

    /**
     * @return true if the reference size contains the given size.
     */
    Q_INVOKABLE static bool contains(const QSizeF& size, const QSizeF& otherSize);

    /**
     * @return true if the reference size contains the given size.
     */
    Q_INVOKABLE static bool contains(const QSize& size, const QSize& otherSize);

    /**
     * @return Area of the provided rectangle or 0 if it is not valid.
     */
    Q_INVOKABLE static qint64 area(const QRect& rect);

    Q_INVOKABLE static QRectF movedInto(const QRectF& rect, const QRectF& target);

    Q_INVOKABLE static QRectF subRect(const QRectF& rect, const QRectF& relativeSubRect);
    Q_INVOKABLE static QRectF unsubRect(const QRectF& rect, const QRectF& relativeSubRect);
    Q_INVOKABLE static QRectF toSubRect(const QRectF& rect, const QRectF& absoluteSubRect);

    Q_INVOKABLE static QPointF corner(const QRectF& rect, Qt::Corner corner);

    Q_INVOKABLE static QPointF closestPoint(
        const QRectF& rect,
        const QPointF& point);
    Q_INVOKABLE static QPointF closestPoint(
        const QPointF& a,
        const QPointF& b,
        const QPointF& point,
        qreal *t);

    Q_INVOKABLE static QRectF rotated(
        const QRectF& rect,
        qreal degrees);
    Q_INVOKABLE static QPointF rotated(
        const QPointF& point,
        const QPointF& center,
        qreal degrees);

    Q_INVOKABLE static QRectF encloseRotatedGeometry(
        const QRectF& enclosingGeometry,
        qreal aspectRatio,
        qreal rotation);

    static void registerQmlType();
};

} // namespace core
} // namespace client
} // namespace nx
