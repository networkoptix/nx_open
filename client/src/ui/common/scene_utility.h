#ifndef QN_SCENE_UTILITY_H
#define QN_SCENE_UTILITY_H

#include <Qt>
#include <QGraphicsView>
#include <QPoint>
#include <QSize>

#include <utils/common/fuzzy.h>

#include "color_transformations.h"
#include "margins.h"

class QRect;
class QRectF;
class QSizeF;
class QTransform;

#ifndef M_PI
#  define M_PI 3.1415926535897932384626433832795
#endif

class SceneUtility {
public:
    /* Some coefficient-wise arithmetic functions follow. */
    static QPointF cwiseMul(const QPointF &l, const QPointF &r);
    static QPointF cwiseDiv(const QPointF &l, const QPointF &r);
    static QSizeF cwiseMul(const QSizeF &l, const QSizeF &r);
    static QSizeF cwiseDiv(const QSizeF &l, const QSizeF &r);
    static MarginsF cwiseMul(const MarginsF &l, const QSizeF &r);
    static MarginsF cwiseDiv(const MarginsF &l, const QSizeF &r);
    static MarginsF cwiseMul(const QSizeF &l, const MarginsF &r);
    static MarginsF cwiseDiv(const QSizeF &l, const MarginsF &r);
    static MarginsF cwiseMul(const MarginsF &l, const MarginsF &r);
    static MarginsF cwiseDiv(const MarginsF &l, const MarginsF &r);
    static QMargins cwiseSub(const QMargins &l, const QMargins &r);
    static QMargins cwiseAdd(const QMargins &l, const QMargins &r);

    static QRectF resizeRect(const QRectF &rect, const QSizeF &size, Qt::WindowFrameSection resizeGrip);
    static QRect resizeRect(const QRect &rect, const QSize &size, Qt::WindowFrameSection resizeGrip);

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

    /**
     * \param point                     Point, treated as a vector.
     * \returns                         Length of the given vector.
     */
    static qreal length(const QPointF &point);

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
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map to scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRectF mapRectToScene(const QGraphicsView *view, const QRect &rect);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map to scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRectF mapRectToScene(const QGraphicsView *view, const QRectF &rect);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map from scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRect mapRectFromScene(const QGraphicsView *view, const QRectF &rect);

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
     * \param maxSize                   Minimal size. 
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode);

    /**
     * Expands an infinitely small size with the given aspect ratio (width to
     * height ratio) to given minimal size.
     * 
     * \param aspectRatio               Aspect ratio.
     * \param minSize                   Minimal size.
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(qreal aspectRatio, const QSizeF &minSize, Qt::AspectRatioMode mode);

    /**
     * Expands an infinitely small rectangle with the given aspect ratio (width to
     * height ratio) to given minimal rectangle.
     * 
     * \param aspectRatio               Aspect ratio.
     * \param minRect                   Minimal rectangle.
     * \param mode                      Aspect ratio mode.
     * \param alignment                 Alignment of the result relative to minimal rectangle.
     */
    static QRectF expanded(qreal aspectRatio, const QRectF &minRect, Qt::AspectRatioMode mode, Qt::Alignment alignment = Qt::AlignCenter);

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
     * \param size                      Size to check for containment.
     * \param otherSize                 Reference size.
     * \returns                         Whether the reference size contains the given size.
     */
    static bool contains(const QSizeF &size, const QSizeF &otherSize);

    /**
     * \param size                      Size to check for containment.
     * \param otherSize                 Reference size.
     * \returns                         Whether the reference size contains the given size.
     */
    static bool contains(const QSize &size, const QSize &otherSize);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param viewportPositionDelta     Move delta, in viewport coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPoint &viewportPositionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param viewportPositionDelta     Move delta, in viewport coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPointF &viewportPositionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param scenePositionDelta        Move delta, in scene coordinates.
     */
    static void moveViewportScene(QGraphicsView *view, const QPointF &scenePositionDelta);

    /**
     * Centers the given viewport on the given position.
     * 
     * \param view                      Graphics view to center viewport of.
     * \param sceneCenter               Position to center on, in scene coordinates.
     */
    static void moveViewportSceneTo(QGraphicsView *view, const QPointF &sceneCenter);

    /**
     * Scales the given viewport.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     * \param anchor                    Transformation anchor.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

    /**
     * Scales the given viewport.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     * \param sceneAnchor               Transformation anchor, in viewport coordinates.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor, const QPoint &viewportAnchor);

    /**
     * Scales the given viewport to the given size.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param size                      Size to scale to.
     * \param mode                      Aspect ratio mode.
     * \param anchor                    Transformation anchor.
     */
    static void scaleViewportTo(QGraphicsView *view, const QSizeF &size, Qt::AspectRatioMode mode, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

    /**
     * \param viewport                 Viewport. Must not be NULL and must actually be a viewport.
     * \returns                        Graphics view of the given viewport.
     */
    static QGraphicsView *view(QWidget *viewport);

};

#endif // QN_SCENE_UTILITY_H
