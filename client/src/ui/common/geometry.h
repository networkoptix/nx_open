#ifndef QN_GEOMETRY_H
#define QN_GEOMETRY_H

#include <QtCore/Qt>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtGui/QPolygonF>

#include <common/common_globals.h>

#include "margins.h"

class QRect;
class QRectF;
class QSizeF;
class QTransform;

class QnGeometry {
public:
    /* Some coefficient-wise arithmetic functions follow. */
    static QPointF cwiseMul(const QPointF &l, const QPointF &r);
    static QPoint cwiseMul(const QPoint &l, const QPoint &r);
    static QPointF cwiseDiv(const QPointF &l, const QPointF &r);
    static QPointF cwiseMul(const QPointF &l, const QSizeF &r);
    static QPoint cwiseMul(const QPoint &l, const QSize &r);
    static QPointF cwiseDiv(const QPointF &l, const QSizeF &r);
    static QSizeF cwiseMul(const QSizeF &l, const QSizeF &r);
    static QSize cwiseMul(const QSize &l, const QSize &r);
    static QSizeF cwiseDiv(const QSizeF &l, const QSizeF &r);
    static QSizeF cwiseMin(const QSizeF &l, const QSizeF &r);
    static QSizeF cwiseMax(const QSizeF &l, const QSizeF &r);
    static MarginsF cwiseMul(const MarginsF &l, const QSizeF &r);
    static MarginsF cwiseDiv(const MarginsF &l, const QSizeF &r);
    static MarginsF cwiseMul(const QSizeF &l, const MarginsF &r);
    static MarginsF cwiseDiv(const QSizeF &l, const MarginsF &r);
    static MarginsF cwiseMul(const MarginsF &l, const MarginsF &r);
    static MarginsF cwiseDiv(const MarginsF &l, const MarginsF &r);
    static QRectF cwiseMul(const QRectF &l, const QSizeF &r);
    static QRectF cwiseDiv(const QRectF &l, const QSizeF &r);
    static QRectF cwiseSub(const QRectF &l, const QRectF &r);
    static QRectF cwiseAdd(const QRectF &l, const QRectF &r);
    static QRectF cwiseDiv(const QRectF &l, qreal r);
    static QRectF cwiseMul(const QRectF &l, qreal r);
    static QMargins cwiseSub(const QMargins &l, const QMargins &r);
    static QMargins cwiseAdd(const QMargins &l, const QMargins &r);

    static int dot(const QPoint &l, const QPoint &r);

    /**
     * \param margins                   Margins.
     * \returns                         Amount by which the size of some rectangle 
     *                                  will change if the given margins are applied to it.
     */
    static QSizeF sizeDelta(const MarginsF &margins);

    /**
     * \param margins                   Margins.
     * \returns                         Amount by which the size of some rectangle 
     *                                  will change if the given margins are applied to it.
     */
    static QSize sizeDelta(const QMargins &margins);

    /**
     * \param                           Size.
     * \returns                         Aspect ratio of the given size.
     */
    static qreal aspectRatio(const QSizeF &size);

    /**
     * \param                           Size.
     * \returns                         Aspect ratio of the given size.
     */
    static qreal aspectRatio(const QSize &size);

    /**
     * \param                           Rectangle.
     * \returns                         Aspect ratio of the given rectangle.
     */
    static qreal aspectRatio(const QRect &rect);

    /**
     * \param                           Rectangle.
     * \returns                         Aspect ratio of the given rectangle.
     */
    static qreal aspectRatio(const QRectF &rect);

    /**
     * \param                           Size.
     * \returns                         Given size converted to a point.
     */
    static QPointF toPoint(const QSizeF &size) {
        return QPointF(size.width(), size.height());
    }

    /**
     * \param                           Size.
     * \returns                         Given size converted to a point.
     */
    static QPoint toPoint(const QSize &size) {
        return QPoint(size.width(), size.height());
    }

    /**
     * \param                           Point.
     * \returns                         Given point converted to a size.
     */
    static QSizeF toSize(const QPointF &point) {
        return QSizeF(point.x(), point.y());
    }

    /**
     * \param                           Point.
     * \returns                         Given point converted to a size.
     */
    static QSize toSize(const QPoint &point) {
        return QSize(point.x(), point.y());
    }

    static qreal dotProduct(const QPointF &a, const QPointF &b);

    /**
     * \param point                     Point, treated as a vector.
     * \returns                         Length of the given vector.
     */
    static qreal length(const QPointF &point);

    /**
     * \param point                     Point, treated as a vector.
     * \returns                         Squared length of the given vector.
     */
    static qreal lengthSquared(const QPointF &point);

    /**
     * \param size                      Size, treated as a vector.
     * \returns                         Length of the given vector.
     */
    static qreal length(const QSizeF &size);

    /**
     * \param point                     Point, treated as a vector.
     * \param                           Unit vector pointing in the same direction as the given vector.
     */
    static QPointF normalized(const QPointF &point);

    /**
     * \param point                     Point, treated as a vector.
     * \returns                         Normal of the same length for the given vector. 
     */
    static QPointF normal(const QPointF &point);

    /**
     * \param point                     Point, treated as a vector.
     * \returns                         Angle between the positive x-axis and the given vector, in radians. 
     *                                  The angle is positive for counter-clockwise angles (upper half-plane, y > 0), 
     *                                  and negative for clockwise angles (lower half-plane, y < 0).
     */
    static qreal atan2(const QPointF &point);

    /**
     * \param polygon                   Polygon.
     * \returns                         Centroid of the set of vertices of the given polygon. 
     *                                  Note that this NOT centroid of the polygon itself.
     */
    static QPointF pointCentroid(const QPolygonF &polygon);

    /**
     * Calculates the factor by which the given size must be scaled to fit into
     * the given bounds.
     * 
     * Note that <tt>Qt::IgnoreAspectRatio</tt> mode is treated the same way
     * as <tt>Qt::KeepAspectRatio</tt> mode.
     * 
     * \param size                      Size to scale.
     * \param bounds                    Size bounds.
     * \param mode                      Aspect ratio mode.
     * \returns                         Scale factor.
     */
    static qreal scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode);

    static QPointF bounded(const QPointF &pos, const QRectF &bounds);
    static QPoint bounded(const QPoint &pos, const QRect &bounds);

    /**
     * Bounds the given size, so that a rectangle of size maxSize would include
     * a rectangle of bounded size.
     *
     * \param size                      Size to bound.
     * \param maxSize                   Maximal size. 
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode);

    /**
     * Expands the given size to given minSize.
     * 
     * \param size                      Size to expand.
     * \param maxSize                   Maximal size. 
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode);

    /**
     * Expands an infinitely small size with the given aspect ratio (width to
     * height ratio) to given maximal size.
     * 
     * \param aspectRatio               Aspect ratio.
     * \param maxSize                   Maximal size.
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(qreal aspectRatio, const QSizeF &maxSize, Qt::AspectRatioMode mode);

    /**
     * Expands an infinitely small rectangle with the given aspect ratio (width to
     * height ratio) to given maximal rectangle.
     * 
     * \param aspectRatio               Aspect ratio.
     * \param maxRect                   Maximal rectangle.
     * \param mode                      Aspect ratio mode.
     * \param alignment                 Alignment of the result relative to minimal rectangle.
     */
    static QRectF expanded(qreal aspectRatio, const QRectF &maxRect, Qt::AspectRatioMode mode, Qt::Alignment alignment = Qt::AlignCenter);

    static QRectF expanded(qreal aspectRatio, const QSizeF &maxSize, const QPointF &center, Qt::AspectRatioMode mode);

    static QRectF scaled(const QRectF &rect, const QSizeF &size, const QPointF &fixedPoint, Qt::AspectRatioMode mode);

    static QRectF aligned(const QSizeF &size, const QRectF &rect, Qt::Alignment alignment = Qt::AlignCenter);

    static QRect aligned(const QSize &size, const QRect &rect, Qt::Alignment alignment = Qt::AlignCenter);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRectF dilated(const QRectF &rect, const QSizeF &amount);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRectF dilated(const QRectF &rect, const MarginsF &amount);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRectF dilated(const QRectF &rect, qreal amount);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRect dilated(const QRect rect, int amount);

    /**
     * Dilates the given size by the given amount.
     * 
     * \param size                      Size to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated size.
     */
    static QSizeF dilated(const QSizeF &size, const MarginsF &amount);

    /**
     * Erodes the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded rectangle.
     */
    static QRectF eroded(const QRectF &rect, const MarginsF &amount);

    /**
     * Erodes the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded rectangle.
     */
    static QRectF eroded(const QRectF &rect, qreal amount);

    /**
     * Erodes the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded rectangle.
     */
    static QRect eroded(const QRect &rect, int amount);

    /**
     * Erodes the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded rectangle.
     */
    static QRect eroded(const QRect &rect, const QMargins &amount);

    /**
     * Erodes the given size by the given amount.
     * 
     * \param size                      Size to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded size.
     */
    static QSizeF eroded(const QSizeF &size, const MarginsF &amount);

    /**
     * Erodes the given size by the given amount.
     * 
     * \param size                      Size to erode.
     * \param amount                    Erosion amount.
     * \returns                         Eroded size.
     */
    static QSize eroded(const QSize &size, const QMargins &amount);

    /**
     * \param size                      Reference size.
     * \param otherSize                 Size to check for containment.
     * \returns                         Whether the reference size contains the given size.
     */
    static bool contains(const QSizeF &size, const QSizeF &otherSize);

    /**
     * \param size                      Reference size.
     * \param otherSize                 Size to check for containment.
     * \returns                         Whether the reference size contains the given size.
     */
    static bool contains(const QSize &size, const QSize &otherSize);

    /**
     * Calculates the area of the given rectangle.
     * 
     * \param rect                      Target rectangle.
     * \returns                         Area of the provided rectangle or 0 if it is not valid.
     */
    static qint64 area(const QRect &rect);

    static QRectF movedInto(const QRectF &rect, const QRectF &target);

    static QRectF subRect(const QRectF &rect, const QRectF &relativeSubRect);
    static QRectF unsubRect(const QRectF &rect, const QRectF &relativeSubRect);
    static QRectF toSubRect(const QRectF &rect, const QRectF &absoluteSubRect);

    static QPointF corner(const QRectF &rect, Qn::Corner corner);

    static QPointF closestPoint(const QRectF &rect, const QPointF &point);
    static QPointF closestPoint(const QPointF &a, const QPointF &b, const QPointF &point, qreal *t);
};

#endif // QN_GEOMETRY_H
